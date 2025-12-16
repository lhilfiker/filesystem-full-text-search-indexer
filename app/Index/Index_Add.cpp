#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <cstring>
#include <filesystem>
#include <string>

int Index::add_new(index_combine_data& index_to_add)
{
  std::error_code ec;
  Log::write(2, "Index: add: first time write.");
  // paths
  size_t file_location = 0;
  // resize files to make enough space to write all the data
  paths_size_buffer = (index_to_add.paths.size() * 2) + index_to_add.paths_size;
  paths_count_size_buffer = index_to_add.paths.size() * 4;

  words_size_buffer =
    index_to_add.words_size +
    (index_to_add.words_and_reversed.size() * WORD_SEPARATOR_SIZE);

  words_f_size_buffer =
    26 *
    (8 + WORDS_F_LOCATION_SIZE); // uint64_t stored as 8 bytes(disk location))
  // + PATH_ID_TYPE stored as 4 bytes(disk id)
  // for each letter in the alphabet.

  reversed_size_buffer =
    index_to_add.words_and_reversed.size() * REVERSED_ENTRY_SIZE;
  additional_size_buffer = 0;
  for (const words_reversed& additional_reversed_counter :
       index_to_add.words_and_reversed) {
    if (additional_reversed_counter.reversed.size() <=
      REVERSED_PATH_LINKS_AMOUNT) {
      continue; // if it fits in only a reverse nlock no additional is
      // requiered.
    }
    additional_size_buffer +=
      (additional_reversed_counter.reversed.size() -
        REVERSED_PATH_LINKS_AMOUNT + ADDITIONAL_PATH_LINKS_AMOUNT - 1) /
      ADDITIONAL_PATH_LINKS_AMOUNT; // celling divsion to round up to
    // the next big number.
  }
  additional_size_buffer *=
    ADDITIONAL_ENTRY_SIZE; // amount of additionals times the bytes per
  // additional

  // resize
  disk_io.unmap();
  resize(CONFIG_INDEX_PATH / "paths.index", paths_size_buffer);
  resize(CONFIG_INDEX_PATH / "paths_count.index", paths_count_size_buffer);
  resize(CONFIG_INDEX_PATH / "words.index", words_size_buffer);
  resize(CONFIG_INDEX_PATH / "words_f.index", words_f_size_buffer);
  resize(CONFIG_INDEX_PATH / "reversed.index", reversed_size_buffer);
  resize(CONFIG_INDEX_PATH / "additional.index", additional_size_buffer);
  disk_io.map(CONFIG_INDEX_PATH);

  // write all paths to disk with a offset in beetween.
  for (const std::string& path : index_to_add.paths) {
    size_t path_length = path.length();
    disk_io.set_path_separator(file_location, path_length);
    file_location += 2;
    disk_io.set_path(file_location, path, path_length);
    file_location += path_length;
  }
  paths_size_buffer = file_location;
  index_to_add.paths.clear(); // free memory as we have written this to memory
  // already and don't need it anymore.

  Log::write(2, "Index: add: paths written.");

  // paths_count
  // write the word count as 4 bytes values each to disk.
  file_location = 0;
  for (size_t i = 0; i < index_to_add.paths_count.size(); ++i) {
    disk_io.set_path_separator(i + 1, index_to_add.paths_count[i]);
    // +1 because path_ids start from 1 and here we start from 0.
    file_location += 4;
  }
  paths_count_size_buffer = file_location;
  index_to_add.paths_count.clear(); // free memory

  Log::write(2, "Index: add: paths_count written.");

  // words & words_f & reversed
  std::vector<WordsFValue> words_f(26);
  char current_char = '0';
  file_location = 0;
  size_t on_disk_id = 0;

  for (const auto& [word, reversed] : index_to_add.words_and_reversed) {
    // check if the first char is different from the last. if so, save the
    // location to words_f. words_reversed is sorted already.
    if (word[0] !=
      current_char) {
      // save file location if a new letter appears.
      current_char = word[0];
      words_f[current_char - 'a'].location = file_location;
      words_f[current_char - 'a'].id = on_disk_id;
    }
    // We use a 1byte seperator beetween each word. We also directly use it to
    // indicate how long the word is.
    size_t word_length = word.length();
    disk_io.set_word_separator(file_location, word_length);
    file_location += WORD_SEPARATOR_SIZE;

    disk_io.set_word(file_location, word, word_length);
    file_location += word_length;
    ++on_disk_id;
  }
  words_size_buffer = file_location;
  Log::write(2, "indexer: add: words written");

  // write words_f
  std::vector<uint32_t> to_set;
  // check words_f. If any character is 0, set it to the value of the next non
  // 0 word. This can happen if no word with a specific letter occured. We set
  // it to the next char start.
  for (int i = 1; i < 26; ++i) {
    // first one is always 0 so we skip it.
    // we add it to a list of items with 0. If we find one with a number we
    // write all in the list to the same number. At the end if there are still 0
    // in the list we write it with the last location of the file.
    if (words_f[i].location == 0) {
      to_set.push_back(i);
    }
    else {
      for (const int& j : to_set) {
        words_f[j] = words_f[i];
      }
      to_set.clear();
    }
  }
  for (const size_t& j :
       to_set) {
    // if there are any left we set them to the end of words
    words_f[j].location = file_location;
    words_f[j].id = on_disk_id;
  }

  disk_io.set_words_f(words_f);
  file_location = 26 * (8 + WORDS_F_LOCATION_SIZE);

  words_f_size_buffer = file_location;
  Log::write(2, "indexer: add: words_f written");

  // reversed & additional
  file_location = 0;
  size_t word_id = 0;
  size_t additional_id = 1;
  for (const words_reversed& reversed : index_to_add.words_and_reversed) {
    // check if we need only a reversed block or also additionals.
    ReversedBlock current_ReversedBlock{};
    if (reversed.reversed.size() <= REVERSED_PATH_LINKS_AMOUNT) {
      // we just need a reversed so we will write that.
      size_t i = 0;
      for (const PATH_ID_TYPE& r_id : reversed.reversed) {
        current_ReversedBlock.ids.path[i] =
          r_id + 1; // paths are indexed from 1
        ++i;
      }
    }
    else {
      // create additional blocks and write if full. At the end write current
      // additional that has not been written.
      size_t additional_i = 0;
      size_t reversed_i = 0;
      AdditionalBlock additional{};
      for (const PATH_ID_TYPE& path_id : reversed.reversed) {
        if (reversed_i < REVERSED_PATH_LINKS_AMOUNT) {
          current_ReversedBlock.ids.path[reversed_i] = path_id + 1;
          ++reversed_i;
          continue;
        }
        if (reversed_i == REVERSED_PATH_LINKS_AMOUNT) {
          current_ReversedBlock.ids.additional[0] = additional_id;
          ++reversed_i;
        }
        if (additional_i ==
          ADDITIONAL_PATH_LINKS_AMOUNT) {
          // the next additional id
          // field.
          additional.ids.additional[0] = additional_id + 1;
          additional_i = 0;

          disk_io.set_additional(additional, additional_id);

          ++additional_id;
          additional = {};
        }
        additional.ids.path[additional_i] = path_id + 1;
        ++additional_i;
      }
      if (additional_i != 0) {
        disk_io.set_additional(additional, additional_id);
        ++additional_id;
      }
    }

    disk_io.set_reversed(current_ReversedBlock, word_id);
    file_location += REVERSED_ENTRY_SIZE;
    ++word_id;
  }
  reversed_size_buffer = file_location;
  Log::write(2, "indexer: add: reversed and additonal written");

  // we do not need to resize because we already know the size accuratly.
  first_time = false;

  if (ec) {
    Log::write(3, "Index: add: error");
    return 1;
  }
  return 0;
}

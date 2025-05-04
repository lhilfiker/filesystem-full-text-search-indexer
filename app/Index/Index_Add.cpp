#include "../Logging/logging.h"
#include "index.h"
#include <cstring>
#include <filesystem>
#include <string>

int Index::add_new(index_combine_data &index_to_add) {
  std::error_code ec;
  Log::write(2, "Index: add: first time write.");
  // paths
  size_t file_location = 0;
  // resize files to make enough space to write all the data
  paths_size_buffer = (index_to_add.paths.size() * 2) + index_to_add.paths_size;
  paths_count_size_buffer = index_to_add.paths.size() * 4;
  words_size_buffer =
      index_to_add.words_size + index_to_add.words_and_reversed.size();
  // words_f_size maybe needs to be extended to allow larger numbers if 8
  // bytes turn out to be too small. maybe automatically resize if running out
  // of space?
  words_f_size_buffer =
      26 *
      (8 + WORDS_F_LOCATION_SIZE); // uint64_t stored as 8 bytes(disk location))
                                   // + PATH_ID_TYPE stored as 4 bytes(disk id)
                                   // for each letter in the alphabet.

  reversed_size_buffer =
      index_to_add.words_and_reversed.size() * REVERSED_ENTRY_SIZE;
  additional_size_buffer = 0;
  for (const words_reversed &additional_reversed_counter :
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
  unmap();
  resize(CONFIG_INDEX_PATH / "paths.index", paths_size_buffer);
  resize(CONFIG_INDEX_PATH / "paths_count.index", paths_count_size_buffer);
  resize(CONFIG_INDEX_PATH / "words.index", words_size_buffer);
  resize(CONFIG_INDEX_PATH / "words_f.index", words_f_size_buffer);
  resize(CONFIG_INDEX_PATH / "reversed.index", reversed_size_buffer);
  resize(CONFIG_INDEX_PATH / "additional.index", additional_size_buffer);
  map();

  // write all paths to disk with a offset in beetween.
  for (const std::string &path : index_to_add.paths) {
    PathOffset offset = {};
    offset.offset = path.length();
    mmap_paths[file_location] = offset.bytes[0];
    ++file_location;
    mmap_paths[file_location] = offset.bytes[1];
    ++file_location;
    for (const char &c : path) {
      mmap_paths[file_location] = c;
      ++file_location;
    }
  }
  paths_size = file_location;
  index_to_add.paths.clear(); // free memory as we have written this to memory
                              // already and don't need it anymore.

  Log::write(2, "Index: add: paths written.");

  // paths_count
  // write the word count as 4 bytes values each to disk.
  file_location = 0;
  for (size_t i = 0; i < index_to_add.paths_count.size(); ++i) {
    PathsCountItem new_paths_count{index_to_add.paths_count[i]};
    for (int j = 0; j < 4; ++j) {
      mmap_paths_count[file_location] = new_paths_count.bytes[j];
      ++file_location;
    }
  }
  paths_count_size = file_location;
  index_to_add.paths_count.clear(); // free memory

  Log::write(2, "Index: add: paths_count written.");

  // words & words_f & reversed
  std::vector<WordsFValue> words_f(26);
  char current_char = '0';
  file_location = 0;
  size_t on_disk_id = 0;

  for (const words_reversed &word : index_to_add.words_and_reversed) {
    // check if the first char is different from the last. if so, save the
    // location to words_f. words_reversed is sorted already.
    if (word.word[0] !=
        current_char) { // save file location if a new letter appears.
      current_char = word.word[0];
      words_f[current_char - 'a'].location = file_location;
      words_f[current_char - 'a'].id = on_disk_id;
    }
    // We use a 1byte seperator beetween each word. We also directly use it to
    // indicate how long the word is.
    size_t word_length = word.word.length();
    if (word_length + 30 >
        254) { // if the word is under 225 in length we can save it directly,
               // if not we set 255 to indicate that we need to count
               // manually. +30 as under is reserved for actual chars.
      mmap_words[file_location] = 255;
    } else {
      mmap_words[file_location] = word_length + 30;
    }
    ++file_location;
    for (const char c : word.word) {
      mmap_words[file_location] = c;
      ++file_location;
    }
    ++on_disk_id;
  }
  words_size = file_location;
  Log::write(2, "indexer: add: words written");

  // write words_f
  std::vector<uint32_t> to_set;
  // check words_f. If any character is 0, set it to the value of the next non
  // 0 word. This can happen if no word with a specific letter occured. We set
  // it to the next char start.
  for (int i = 1; i < 26; ++i) { // first one is always 0 so we skip it.
    // we add it to a list of items with 0. If we find one with a number we
    // write all in the list to the same number. At the end if there are still 0
    // in the list we write it with the last location of the file.
    if (words_f[i].location == 0) {
      to_set.push_back(i);
    } else {
      for (const int &j : to_set) {
        words_f[j] = words_f[i];
      }
      to_set.clear();
    }
  }
  for (const size_t &j :
       to_set) { // if there are any left we set them to the end of words
    words_f[j].location = file_location;
    words_f[j].id = on_disk_id;
  }

  file_location = 0;
  for (const WordsFValue &word_f : words_f) {
    std::memcpy(&mmap_words_f[file_location], &word_f.bytes[0],
                (8 + WORDS_F_LOCATION_SIZE));
    file_location += (8 + WORDS_F_LOCATION_SIZE);
  }

  words_f_size = file_location;
  Log::write(2, "indexer: add: words_f written");

  // reversed & additional
  file_location = 0;
  size_t additional_file_location = 0;
  size_t additional_id = 1;
  for (const words_reversed &reversed : index_to_add.words_and_reversed) {
    // check if we need only a reversed block or also additionals.
    ReversedBlock current_ReversedBlock{};
    if (reversed.reversed.size() <= REVERSED_PATH_LINKS_AMOUNT) {
      // we just need a reversed so we will write that.
      size_t i = 0;
      for (const PATH_ID_TYPE &r_id : reversed.reversed) {
        current_ReversedBlock.ids.path[i] =
            r_id + 1; // paths are indexed from 1
        ++i;
      }
    } else {
      // create additional blocks and write if full. At the end write current
      // additional that has not been written.
      size_t additional_i = 0;
      size_t reversed_i = 0;
      AdditionalBlock additional{};
      for (const PATH_ID_TYPE &path_id : reversed.reversed) {
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
            ADDITIONAL_PATH_LINKS_AMOUNT) { // the next additional id
                                            // field.
          additional.ids.additional[0] = additional_id + 1;
          additional_i = 0;

          uint16_t byte_counter = 0;
          for (const char &byte : additional.bytes) {
            mmap_additional[additional_file_location] = byte;
            ++additional_file_location;
          }
          ++additional_id;
          additional = {};
        }
        additional.ids.path[additional_i] = path_id + 1;
        ++additional_i;
      }
      if (additional_i != 0) {
        for (const char &byte : additional.bytes) {
          mmap_additional[additional_file_location] = byte;
          ++additional_file_location;
        }
        ++additional_id;
      }
    }

    // write reversed block
    for (int i = 0; i < REVERSED_ENTRY_SIZE; ++i) {
      mmap_reversed[file_location] = current_ReversedBlock.bytes[i];
      ++file_location;
    }
  }
  reversed_size = file_location;
  Log::write(2, "indexer: add: reversed and additonal written");

  // we do not need to resize because we already know the size accuratly.
  first_time = false;

  if (ec) {
    Log::write(3, "Index: add: error");
    return 1;
  }
  return 0;
}

#include "functions.h"
#include "lib/mio.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

union WordsFValue {
  uint64_t value;
  unsigned char bytes[8];
};

union ReversedBlock {
  uint16_t ids[5];
  unsigned char bytes[10];
};

union AdditionalBlock {
  uint16_t ids[25];
  unsigned char bytes[50];
};

union PathOffset {
  uint16_t offset;
  unsigned char bytes[2];
};

bool Index::is_config_loaded = false;
bool Index::is_mapped = false;
bool Index::first_time = false;
int Index::buffer_size = 0;
std::filesystem::path Index::index_path;
int Index::paths_size = 0;
int Index::paths_size_buffer = 0;
int Index::paths_count_size = 0;
int Index::paths_count_size_buffer = 0;
int Index::words_size = 0;
int Index::words_size_buffer = 0;
int Index::words_f_size = 0;
int Index::words_f_size_buffer = 0;
int Index::reversed_size = 0;
int Index::reversed_size_buffer = 0;
int Index::additional_size = 0;
int Index::additional_size_buffer = 0;
mio::mmap_sink Index::mmap_paths;
mio::mmap_sink Index::mmap_paths_count;
mio::mmap_sink Index::mmap_words;
mio::mmap_sink Index::mmap_words_f;
mio::mmap_sink Index::mmap_reversed;
mio::mmap_sink Index::mmap_additional;

void Index::save_config(const std::filesystem::path &config_index_path,
                        const int config_buffer_size) {
  index_path = config_index_path;
  buffer_size = config_buffer_size;
  if (buffer_size < 10000) {
    buffer_size = 10000;
    log::write(3, "Index: save_config: buffer size needs to be atleast 10000. "
                  "setting it to 10000.");
  }
  if (buffer_size > 10000000) {
    buffer_size = 10000000;
    log::write(3, "Index: save_config: buffer size can not be larger than "
                  "~10MB, setting it to ~10MB");
  }
  is_config_loaded = true;
  log::write(1, "Index: save_config: saved config successfully.");
  return;
}

bool Index::is_index_mapped() { return is_mapped; }

void Index::resize(const std::filesystem::path &path_to_resize,
                   const int size) {
  std::error_code ec;
  std::filesystem::resize_file(path_to_resize, size);
  if (ec) {
    log::error("Index: resize: could not resize file at path: " +
               (path_to_resize).string() +
               " with size: " + std::to_string(size) + ".");
  }
  log::write(1, "indexer: resize: resized file successfully.");
  return;
}

int Index::unmap() {
  std::error_code ec;
  if (mmap_paths.is_mapped()) {
    mmap_paths.unmap();
  }
  if (mmap_paths_count.is_mapped()) {
    mmap_paths_count.unmap();
  }
  if (mmap_words.is_mapped()) {
    mmap_words.unmap();
  }
  if (mmap_words_f.is_mapped()) {
    mmap_words_f.unmap();
  }
  if (mmap_reversed.is_mapped()) {
    mmap_reversed.unmap();
  }
  if (mmap_additional.is_mapped()) {
    mmap_additional.unmap();
  }
  if (ec) {
    log::write(4, "Index: unmap: error when unmapping index files.");
    return 1;
  }
  log::write(1, "Index: unmap: unmapped all successfully.");
  return 0;
}

int Index::map() {
  std::error_code ec;
  if (!mmap_paths.is_mapped()) {
    mmap_paths = mio::make_mmap_sink((index_path / "paths.index").string(), 0,
                                     mio::map_entire_file, ec);
  }
  if (!mmap_paths_count.is_mapped()) {
    mmap_paths_count =
        mio::make_mmap_sink((index_path / "paths_count.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_words.is_mapped()) {
    mmap_words = mio::make_mmap_sink((index_path / "words.index").string(), 0,
                                     mio::map_entire_file, ec);
  }
  if (!mmap_words_f.is_mapped()) {
    mmap_words_f = mio::make_mmap_sink((index_path / "words_f.index").string(),
                                       0, mio::map_entire_file, ec);
  }
  if (!mmap_reversed.is_mapped()) {
    mmap_reversed = mio::make_mmap_sink(
        (index_path / "reversed.index").string(), 0, mio::map_entire_file, ec);
  }
  if (!mmap_additional.is_mapped()) {
    mmap_additional =
        mio::make_mmap_sink((index_path / "additional.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (ec) {
    log::write(3, "Index: map: error when mapping index files.");
    return 1;
  }
  log::write(1, "Index: map: mapped all successfully.");
  return 0;
}

int Index::get_actual_size(const mio::mmap_sink &mmap) {
  std::error_code ec;
  if (!mmap.is_mapped()) {
    log::write(3, "Index: get_actual_size: not mapped. can not count.");
    return -1;
  }
  int size = 0;
  for (const char &c : mmap) {
    if (c == '\0')
      break;
    ++size;
  }
  if (ec) {
    log::write(3, "Index: get_actual_size: error when couting actual size");
    return -1;
  }
  log::write(1, "Index: get_actual_size: successfully counted size: " +
                    std::to_string(size));
  return size;
}

void Index::check_files() {
  if (!is_config_loaded) {
    log::error("Index: check_files: config not loaded. can not continue.");
  }
  log::write(1, "Index: check_files: checking files.");
  std::error_code ec;
  if (!std::filesystem::is_directory(index_path)) {
    log::write(1, "Index: check_files: creating index directory.");
    std::filesystem::create_directories(index_path);
  }
  if (!std::filesystem::exists(index_path / "paths.index") ||
      helper::file_size(index_path / "paths.index") == 0 ||
      !std::filesystem::exists(index_path / "paths_count.index") ||
      helper::file_size(index_path / "paths_count.index") == 0 ||
      !std::filesystem::exists(index_path / "words.index") ||
      helper::file_size(index_path / "words.index") == 0 ||
      !std::filesystem::exists(index_path / "words_f.index") ||
      helper::file_size(index_path / "words_f.index") == 0 ||
      !std::filesystem::exists(index_path / "reversed.index") ||
      helper::file_size(index_path / "reversed.index") == 0 ||
      !std::filesystem::exists(index_path / "additional.index") ||
      helper::file_size(index_path / "additional.index") == 0) {
    first_time = true;
    log::write(
        1,
        "Index: check_files: index files damaged / not existing, recreating.");
    std::ofstream{index_path / "paths.index"};
    std::ofstream{index_path / "paths_count.index"};
    std::ofstream{index_path / "words.index"};
    std::ofstream{index_path / "words_f.index"};
    std::ofstream{index_path / "reversed.index"};
    std::ofstream{index_path / "additional.index"};
  }
  if (ec) {
    log::error("Index: check_files: error accessing/creating index files in " +
               (index_path).string() + ".");
  }
  return;
}

int Index::initialize() {
  if (!is_config_loaded) {
    log::write(4, "Index: initialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  std::error_code ec;
  unmap();       // unmap anyway incase they are already mapped.
  check_files(); // check if index files exist and create them.
  map();         // ignore error here as it might fail if file size is 0.
  // get actual sizes of the files to reset buffer.
  if (int paths_size = get_actual_size(mmap_paths);
      paths_size == -1 && helper::file_size(index_path / "paths.index") > 0)
    paths_size = 0;
  if (int paths_count_size = get_actual_size(mmap_paths);
      paths_count_size == -1 &&
      helper::file_size(index_path / "paths_count.index") > 0)
    paths_count_size = 0;
  if (int words_size = get_actual_size(mmap_words);
      words_size == -1 && helper::file_size(index_path / "words.index") > 0)
    words_size = 0;
  if (int words_f_size = get_actual_size(mmap_words_f);
      words_f_size == -1 && helper::file_size(index_path / "words_f.index") > 0)
    words_f_size = 0;
  if (int reversed_size = get_actual_size(mmap_reversed);
      reversed_size == -1 &&
      helper::file_size(index_path / "reversed.index") > 0)
    reversed_size = 0;
  if (int additional_size = get_actual_size(mmap_additional);
      additional_size == -1 &&
      helper::file_size(index_path / "additional.index") > 0)
    additional_size = 0;
  if (paths_size == -1 || words_size == -1 || words_f_size == -1 ||
      reversed_size == -1 || additional_size == -1) {
    log::write(4, "Index: initialize: could not get actual size of index "
                  "files, exiting.");
    return 1;
  }
  paths_size_buffer = paths_size + buffer_size;
  paths_count_size_buffer = paths_count_size + buffer_size;
  words_size_buffer = words_size + buffer_size;
  words_f_size_buffer = words_f_size + buffer_size;
  reversed_size_buffer = reversed_size + buffer_size;
  additional_size_buffer = additional_size + buffer_size;
  if (unmap() == 1) { // unmap to resize
    log::write(
        4,
        "Index: initialize: could not unmap, see above log message, exiting.");
    return 1;
  }
  // resize actual size + buffer size
  resize(index_path / "paths.index", paths_size_buffer);
  resize(index_path / "paths_count.index", paths_count_size_buffer);
  resize(index_path / "words.index", words_size_buffer);
  resize(index_path / "words_f.index", words_f_size_buffer);
  resize(index_path / "reversed.index", reversed_size_buffer);
  resize(index_path / "additional.index", additional_size_buffer);
  // map after resizing
  if (map() == 1) { // map files
    log::write(
        4, "Index: initialize: could not map, see above log message, exiting.");
    return 1;
  }
  is_mapped = true;
  return 0;
}

int Index::uninitialize() {
  if (!is_config_loaded) {
    log::write(4, "Index: uninitialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  // write caches, etc...
  if (unmap() == 1) {
    log::write(4, "Index: uninitialize: could not unmap.");
    return 1;
  }
  return 0;
}

int Index::add_new(index_combine_data &index_to_add) {
  std::error_code ec;
  log::write(2, "Index: add: first time write.");
  // paths
  uint64_t file_location = 0;
  // resize files to make enough space to write all the data
  paths_size_buffer = (index_to_add.paths.size() * 2) + index_to_add.paths_size;
  paths_count_size_buffer = index_to_add.paths_count_size;
  words_size_buffer =
      index_to_add.words_size + index_to_add.words_and_reversed.size();
  // words_f_size maybe needs to be extended to allow larger numbers if 8
  // bytes turn out to be too small. maybe automatically resize if running out
  // of space?
  words_f_size_buffer = 26 * 8; // uint64_t stored as 8 bytes(64 bits) for
                                // each letter in the alphabet.
  reversed_size_buffer = index_to_add.words_and_reversed.size() *
                         10; // each word id has a 10 byte block.
  additional_size_buffer = 0;
  for (const words_reversed &additional_reversed_counter :
       index_to_add.words_and_reversed) {
    if (additional_reversed_counter.reversed.size() < 5)
      continue; // if under 4 words, no additional is requiered.
    additional_size_buffer +=
        (additional_reversed_counter.reversed.size() + 19) / 24;
  }
  additional_size_buffer *= 50;

  // resize
  unmap();
  resize(index_path / "paths.index", paths_size_buffer);
  resize(index_path / "paths_count.index", paths_count_size_buffer);
  resize(index_path / "words.index", words_size_buffer);
  resize(index_path / "words_f.index", words_f_size_buffer);
  resize(index_path / "reversed.index", reversed_size_buffer);
  resize(index_path / "additional.index", additional_size_buffer);
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

  log::write(2, "Index: add: paths written.");

  // paths_count
  // write the word count as 4 bytes values each to disk.
  file_location = 0;
  for (const uint32_t &path_count : index_to_add.paths_count) {
    mmap_paths_count[file_location] = (path_count >> 24) & 0xFF;
    mmap_paths_count[file_location + 1] = (path_count >> 16) & 0xFF;
    mmap_paths_count[file_location + 2] = (path_count >> 8) & 0xFF;
    mmap_paths_count[file_location + 3] = path_count & 0xFF;
    file_location += 4;
  }
  paths_count_size = file_location;
  index_to_add.paths_count.clear(); // free memory

  log::write(2, "Index: add: paths_count written.");

  // words & words_f & reversed
  std::vector<WordsFValue> words_f(26);
  char current_char = '0';
  file_location = 0;

  for (const words_reversed &word : index_to_add.words_and_reversed) {
    // check if the first char is different from the last. if so, save the
    // location to words_f. words_reversed is sorted already.
    if (word.word[0] !=
        current_char) { // save file location if a new letter appears.
      current_char = word.word[0];
      words_f[current_char - 'a'].value = file_location;
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
    for (const char &c : word.word) {
      mmap_words[file_location] = c - 'a';
      ++file_location;
    }
  }
  words_size = file_location;
  log::write(2, "indexer: add: words written");

  // write words_f
  std::vector<int> to_set;
  // check words_f. If any character is 0, set it to the value of the next non
  // 0 word. This can happen if no word with a specific letter occured. We set
  // it to the next char start.
  for (int i = 1; i < 26; ++i) { // first one is always 0 so we skip it.
    // we add it to a list of items with 0. If we find one with a number we
    // write all in the list to the same number. At the end if there are still 0
    // in the list we write it with the last location of the file.
    if (words_f[i].value == 0) {
      to_set.push_back(i);
    } else {
      for (const int &j : to_set) {
        words_f[j].value = words_f[i].value;
      }
      to_set.clear();
    }
  }
  for (const int &j :
       to_set) { // if there are any left we set them to the end of words
    words_f[j].value = file_location;
  }

  file_location = 0;
  for (const WordsFValue &word_f : words_f) {
    std::memcpy(&mmap_words_f[file_location], &word_f.bytes[0], 8);
    file_location += 8;
  }

  words_f_size = file_location;
  log::write(2, "indexer: add: words_f written");

  // reversed & additional
  file_location = 0;
  size_t additional_file_location = 0;
  size_t additional_id = 1;
  for (const words_reversed &reversed : index_to_add.words_and_reversed) {
    // check if we need only a reversed block or also additionals.
    ReversedBlock current_ReversedBlock{};
    if (reversed.reversed.size() <= 4) {
      // we just need a reversed so we will write that.
      int i = 0;
      for (const uint32_t &r_id : reversed.reversed) {
        current_ReversedBlock.ids[i] = r_id + 1; // paths are indexed from 1
        ++i;
      }
    } else {
      // create additional blocks and write if full. At the end write current
      // additional that has not been written.
      int additional_i = 0;
      int reversed_i = 0;
      AdditionalBlock additional{};
      for (const uint32_t &path_id : reversed.reversed) {
        if (reversed_i < 4) {
          current_ReversedBlock.ids[reversed_i] = path_id + 1;
          ++reversed_i;
          continue;
        }
        if (reversed_i == 4) {
          current_ReversedBlock.ids[4] = additional_id;
          ++reversed_i;
        }
        if (additional_i == 24) { // the next additional id field.
          additional.ids[24] = additional_id + 1;
          additional_i = 0;
          for (const char &byte : additional.bytes) {
            mmap_additional[additional_file_location] = byte;
            ++additional_file_location;
          }
          ++additional_id;
          additional = {};
        }
        additional.ids[additional_i] = path_id + 1;
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
    for (int i = 0; i < 10; ++i) {
      mmap_reversed[file_location] = current_ReversedBlock.bytes[i];
      ++file_location;
    }
  }
  reversed_size = file_location;
  log::write(2, "indexer: add: reversed and additonal written");

  // we do not need to resize because we already know the size accuratly.
  first_time = false;

  if (ec) {
    log::write(3, "Index: add: error");
    return 1;
  }
  return 0;
}

void add_reversed_to_word(index_combine_data &index_to_add, uint64_t &on_disk_id, std::vector<Transaction> &transactions, uint64_t &additional_new_needed_size) {

}

int Index::merge(index_combine_data &index_to_add) {
  log::write(2, "indexer: add: adding to existing index.");
  map();

  std::vector<Transaction> transactions;
  std::vector<Insertion> words_insertions;
  std::vector<Insertion> reversed_insertions;
  uint64_t additional_new_needed_size = 0;

  size_t paths_l_size = index_to_add.paths.size();

  // So we can easily covert disk or local IDs fast.
  struct PathsMapping {
    std::unordered_map<uint16_t, uint16_t> by_local;
    std::unordered_map<uint16_t, uint16_t> by_disk;
  };
  PathsMapping paths_mapping;

  // convert local paths to a map with value path and key id. We will go through
  // each path on disk and see if it is in the local index. Using a unordered
  // map we can check that very fast and get the local id so we can map the
  // local id to disk id and vica versa.
  // TODO: more memory efficent converting. Currently this would double the RAM
  // usage and violate the memory limit set by the user.
  std::unordered_map<std::string, uint16_t> paths_search;
  size_t path_insertion_count = 0;
  for (const std::string &s : index_to_add.paths) {
    paths_search[s] = path_insertion_count;
    ++path_insertion_count;
  }
  index_to_add.paths.clear();

  // go through index on disk and map disk Id to local Id.
  uint64_t on_disk_count = 0;
  uint64_t on_disk_id = 0;
  uint16_t next_path_end = 0; // if 0 the next 2 values are the header.

  // paths_size is the count of bytes of the index on disk.
  while (on_disk_count < paths_size) {
    if (on_disk_count + 1 <
        paths_size) { // we read 1 byte ahead for the offset to prevent
                      // accessing invalid data. The index format would allow it
                      // but it could be corrupted and not detected.
      PathOffset path_offset;
      // we read the offset so we know how long the path is and where the next
      // path starts.
      path_offset.bytes[0] = mmap_paths[on_disk_count];
      ++on_disk_count;
      path_offset.bytes[1] = mmap_paths[on_disk_count];
      next_path_end = path_offset.offset;
      ++on_disk_count;

      if (on_disk_count + next_path_end <
          paths_size) { // check if we can read the whole path based on the offset.
        // refrence the path to a string and then search in the unordered map we created earlier.
        std::string path_to_compare(&mmap_paths[on_disk_count], next_path_end);
        if (paths_search.find(path_to_compare) != paths_search
                .end()) { // check if the disk path is found in memory index.

          paths_mapping.by_local[paths_search[path_to_compare]] =
              on_disk_id; // map disk id to memory id.
          paths_mapping.by_disk[on_disk_id] =
              paths_search[path_to_compare]; // map memory id to disk id.
          paths_search.erase(
              path_to_compare); // remove already found items so we know later
                                // which ones are not on disk.
        }
        on_disk_count += next_path_end;
      } else {
        log::error("Index: Combine: invalid path content length. Aborting. "
                   "The Index could be corrupted.");
      }
      ++on_disk_id;

    } else {
      // Abort. A corrupted index would mess things up. If the corruption
      // could not get detectet or fixed before here it is most likely broken.
      log::error("Index: Combine: invalid path content length indicator. "
                 "Aborting. The Index could be corrupted.");
    }
  }

  // go through all remaining paths_search elements, add a transaction and add
  // a new id to paths_mapping and create a resize transaction.
  size_t needed_space = 0;
  std::string paths_add_content = "";
  for (const auto &[key, value] : paths_search) {
    paths_mapping.by_local[value] = on_disk_id;
    paths_mapping.by_disk[on_disk_id] = value;
    // add all 2 byte offset + path to a string because all missing paths will
    // be added one after another
    PathOffset path_offset;
    path_offset.offset = key.length();
    paths_add_content += path_offset.bytes[0];
    paths_add_content += path_offset.bytes[1];
    paths_add_content += key;
    ++on_disk_id;
  }
  // write it at the end at once if needed.
  if (needed_space != 0) {
    // resize all paths + offset fit.
    Transaction resize_transaction{0, 0, 0, 0, 2, paths_size + needed_space};
    transactions.push_back(resize_transaction);
    // write it to the now free space at the end of the file.
    Transaction to_add_path_transaction{0,
                                        0,
                                        static_cast<uint64_t>(paths_size - 1),
                                        0,
                                        1,
                                        paths_add_content.length(),
                                        paths_add_content};
    transactions.push_back(to_add_path_transaction);
  }
  paths_add_content = "";
  paths_search.clear();

  // copy words_f into memory
  std::vector<WordsFValue> words_f(26);
  for (int i = 0; i < 26; ++i) {
    std::memcpy(words_f[i].bytes, &mmap_words_f[i * 8], 8);
  }

  // local index words needs to have atleast 1 value. Is checked by LocalIndex
  // add_to_disk.
  on_disk_count = 0;
  on_disk_id = 0;
  size_t local_word_count = 0;
  size_t local_word_length = index_to_add.words_and_reversed[0].word.length();
  char current_first_char = 'a';
  char local_first_char =
      index_to_add.words_and_reversed[local_word_count].word[0];

  uint64_t disk_additional_ids = (additional_size / 50) + 1;

  // go through each word on disk. First we read the offset and compare its
  // value to the length of the current local word. Because both are sorted we
  // can compare them to see if it is the same, passed or will first come.

  // This can't work. If we just skip a word just because it's not the same
  // length as current local word we never detect if we are past a word if no
  // word comes with the same length or we are insert it at the wrong place.

  // Move parts when a word is found and when it is not found into their own
  // functions. Then if the length is not the same we just reach each character
  // until its bytes values are not the same. If it is larger it means we are
  // passed and we should call new word function. if not we skip it. If it is
  // the same length and same word we will call word found function.

  while(on_disk_count + 1 < words_size) {
    // If the current word first char is different we use words_f to set the location to the start of that char.
    if (current_first_char < local_first_char) {
      if (words_f[current_first_char - 'a'].value < words_size) {
        on_disk_count = words_f[current_first_char - 'a'].value;
      } else {
        // This should not happen. Index is corrupted.
        log::error("Index: Combine: Words_f char value is higher than words index size. This means the index is corrupted. Reset the index and report this problem.");
      }
    }

    // read the one byte word sperator.
    uint8_t word_seperator = mmap_words[on_disk_count];
    if (word_seperator < 31 || word_seperator + on_disk_count > words_size) { // 0-30 is reserved. if it is higher it is for seperator. If the seperator here is 0-30 the index is corrupted.
      log::error("Index: Combine: word seperator is invalid. This means the index is most likely corrupted. Stopping to protect the index.");
    }
    if (local_first_char != mmap_words[on_disk_count + 1]) { // + 1 because of the word seperator
      current_first_char = mmap_words[on_disk_count + 1];
      continue; // at the start we will then go to the correct first char of that word and repeat the whole block. 
    }
    if (word_seperator == 255) { // This means the word is larger than 255 bytes. We need to count it manually until we reach another 30< byte.
      // go through start position until end position comparing each char until either it is smaller or bigger. then just try to find the next seperator.
      // update word_lentgh and count and first char.
      continue;
    }

    for (int i = 0; i < word_seperator; ++i) {
      if (local_word_length < i) { // if the local word is smaller then the on disk one and we already reached this point we know the disk word is bigger.
        // insert a new word and reversed and additional. Update words_f if needed.
      }
      if (mmap_words[on_disk_count + 1 + i] > index_to_add.words_and_reversed[local_word_count].word[i]) {// if the on disk char is bigger it means we went already past the word.
        // insert a new word and reversed and additional. Update words_f if needed.
      }
      if (mmap_words[on_disk_count + 1 + i] < index_to_add.words_and_reversed[local_word_count].word[i]) {
        // this means the on disk is smaller than the local. Skip to the next word.
        break;
      }
      if (word_seperator == i && word_seperator == local_word_count) { // if we reach this point and they are the same word we found the word.
        // update reversed and additionals if needed.
        add_reversed_to_word(index_to_add, on_disk_id, transactions, additional_new_needed_size);
      }
    }
    ++local_word_count;
    local_word_length = index_to_add.words_and_reversed[local_word_count].word.length();
  }

/*  while (on_disk_count < words_size) {
    if (current_first_char < local_first_char) {
      on_disk_count = words_f[current_first_char - 'a'].value;
    }
    // read word length
    uint8_t word_seperator = mmap_words[on_disk_count];
    if (on_disk_count + word_seperator - 29 > words_size)
      break; // if we cant read the whole word break. Maybe call a index check
             // as this would mean it is corrupted.
    current_first_char = mmap_words[on_disk_count + 1];
    if (word_seperator - 30 != local_word_length &&
        word_seperator != 255) { // skip word if not same length
      ++on_disk_id;
      on_disk_count += word_seperator - 29; // include word seperator
      continue;
    }
    on_disk_count += 2;
    current_word += current_first_char;
    if (word_seperator == 255) { // read until we have a number over 30
      // when it will be basic string, read until 235 in one batch.
      while (true) {
        if (mmap_words[on_disk_count] > 29)
          break;
        current_word += mmap_words[on_disk_count];
        ++on_disk_count;
      }
    } else {
      // we will read the chars 1 by 1 to add it to the wstring for now. when
      // we have changed the type from wstring to basic string we can copy it.
      for (; on_disk_count < on_disk_count + word_seperator - 1;
           ++on_disk_count) {
        current_word += mmap_words[on_disk_count];
      }
    }

    if (current_word ==
        index_to_add.words_and_reversed[local_word_count].word) {
      // word found. insert into reversed and additional. create new
      // additional if needed.
      //
      // TODO: Overwrite path word count with inputet value.

      if (on_disk_id * 10 + 10 >= reversed_size) {
        log::error(
            "Index: Reversed out of range. Index corrupted."); // index most
                                                               // likely
                                                               // corrupted.
      }
      ReversedBlock *disk_reversed =
          reinterpret_cast<ReversedBlock *>(&mmap_reversed[on_disk_id * 10]);
      bool current_has_place = false;
      for (int i = 0; i < 4; ++i) {
        if (disk_reversed->ids[i] != 0) {
          index_to_add.words_and_reversed[local_word_count].reversed.erase(
              paths_mapping.by_disk[disk_reversed->ids[i]]);
        } else {
          current_has_place = true;
        }
      }
      if (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
              0 &&
          current_has_place) {
        for (int i = 0; i < 4; ++i) {
          if (disk_reversed->ids[i] == 0) {
            Transaction reversed_add_transaction{
                0, 3, (on_disk_id * 10) + (i * 2), 0, 1, 2};
            const auto &r_id =
                *index_to_add.words_and_reversed[local_word_count]
                     .reversed.begin();
            PathOffset content;
            content.offset = paths_mapping.by_local[r_id];
            reversed_add_transaction.content =
                content.bytes[0] + content.bytes[1];
            index_to_add.words_and_reversed[local_word_count].reversed.erase(
                r_id);
            transactions.push_back(reversed_add_transaction);
          }
        }
      }
      uint16_t last_additional = 0;
      if (index_to_add.words_and_reversed[local_word_count].reversed.size() !=
          0) {
        if (uint16_t additional_id = disk_reversed->ids[4];
            additional_id != 0) {
          while (true) {
            AdditionalBlock *disk_additional =
                reinterpret_cast<AdditionalBlock *>(
                    &mmap_additional[(additional_id - 1) * 50]);
            current_has_place = false;
            for (int i = 0; i < 24; ++i) {
              if (disk_additional->ids[i] != 0) {
                index_to_add.words_and_reversed[local_word_count]
                    .reversed.erase(
                        paths_mapping.by_disk[disk_additional->ids[i]]);
              } else {
                current_has_place = true;
              }
            }
            if (index_to_add.words_and_reversed[local_word_count]
                        .reversed.size() != 0 &&
                current_has_place) {
              for (int i = 0; i < 24; ++i) {
                if (disk_additional->ids[i] == 0) {
                  Transaction additional_add_transaction{
                      0, 4, (on_disk_id * 50) + (i * 2), 0, 1, 2};
                  PathOffset content;
                  const auto &r_id =
                      *index_to_add.words_and_reversed[local_word_count]
                           .reversed.begin();
                  content.offset = paths_mapping.by_local[r_id];
                  additional_add_transaction.content =
                      content.bytes[0] + content.bytes[1];
                  index_to_add.words_and_reversed[local_word_count]
                      .reversed.erase(r_id);
                  transactions.push_back(additional_add_transaction);
                }
              }
            }
            if (index_to_add.words_and_reversed[local_word_count]
                        .reversed.size() != 0 &&
                disk_additional->ids[24] != 0) {
              additional_id = disk_additional->ids[24];
            } else if (index_to_add.words_and_reversed[local_word_count]
                               .reversed.size() != 0 &&
                       disk_additional->ids[24] == 0) {
              last_additional = (on_disk_id * 50) + (24 * 2);
              break;
            } else {
              break;
            }
          }
        }
        if (disk_reversed->ids[4] == 0 || last_additional != 0) {
          std::vector<AdditionalBlock> new_additionals;

          PathOffset last_additional_id;
          last_additional_id.offset = disk_additional_ids;

          Transaction additional_add_transaction{0, 0, 0, 0, 1, 2};
          if (last_additional == 0) {
            additional_add_transaction.header.index_type = 3;
            additional_add_transaction.header.location = (on_disk_id * 10) + 8;
          } else {
            additional_add_transaction.header.index_type = 4;
            additional_add_transaction.header.location = (on_disk_id * 50) + 48;
          }
          additional_add_transaction.content =
              last_additional_id.bytes[0] + last_additional_id.bytes[1];
          transactions.push_back(additional_add_transaction);

          // create new additonals and create insertions at the end.
          size_t current_additional = 0;
          AdditionalBlock current_additional_block{};
          for (const uint16_t &p_id :
               index_to_add.words_and_reversed[local_word_count].reversed) {
            if (current_additional == 24) {
              ++disk_additional_ids;
              current_additional_block.ids[24] = disk_additional_ids;
              new_additionals.push_back(current_additional_block);
              current_additional_block = {};
              current_additional = 0;
            }
            current_additional_block.ids[current_additional] = p_id;
            ++current_additional;
          }
          if (current_additional_block.ids[0] != 0) {
            if (current_additional_block.ids[24] != 0) {
              current_additional_block.ids[24] = 0;
            }
            new_additionals.push_back(current_additional_block);
          }

          std::string insert_content = "";
          for (const AdditionalBlock &a_block : new_additionals) {
            for (int i = 0; i < 50; ++i) {
              insert_content +=
                  a_block.bytes[i]; // TODO: more efficent copying.
            }
          }

          additional_new_needed_size += insert_content.length();

          Transaction additional_append_transaction{
              0,
              4,
              additional_size + additional_new_needed_size,
              0,
              1,
              insert_content.length(),
              insert_content};
          transactions.push_back(additional_append_transaction);
        }
      }

      ++local_word_count;
      local_first_char =
          index_to_add.words_and_reversed[local_word_count].word[0];
      if (local_word_count < index_to_add.words_and_reversed.size())
        break; // done with all.
    } else if (current_word >
               index_to_add.words_and_reversed[local_word_count].word) {
      // insert a new word before this word. create new reversed and if needed
      // additional. update words_f: update all after this character by how
      // long it is + seperator. do in one.
      ++local_word_count;
      local_first_char =
          index_to_add.words_and_reversed[local_word_count].word[0];
      if (local_word_count < index_to_add.words_and_reversed.size())
        break; // done with all.
    }

    ++on_disk_id;
  }
  for (; local_word_count < index_to_add.words_and_reversed.size();
       ++local_word_count) {
    // insert the remaining at the end. update words_f if needed.
  }
  */

  // resize additional based on additional_new_needed_size
  //
  return 0;
}

int Index::add(index_combine_data &index_to_add) {
  std::error_code ec;
  if (first_time) {
    return add_new(index_to_add);
  } else {
    return merge(index_to_add);
  }
  return 0;
}

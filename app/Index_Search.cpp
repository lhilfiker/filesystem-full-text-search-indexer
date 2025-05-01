#include "functions.h"
#include "lib/mio.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

std::vector<uint64_t> Index::path_ids_from_word_id(uint64_t word_id) {
  std::vector<uint64_t> path_ids;
  if ((word_id * 10) + 10 > reversed_size) {
    log::error("Index: path_ids_from_word_id: to search word id would be at "
               "nonexisting location. Index most likely corrupt. Exiting");
  }
  // load the reversed block into memory.
  ReversedBlock *disk_reversed =
      reinterpret_cast<ReversedBlock *>(&mmap_reversed[word_id * 10]);
  for (int i = 0; i < 4; ++i) {
    if (disk_reversed->ids[i] != 0) {
      path_ids.push_back(disk_reversed->ids[i]);
    }
  }

  if (disk_reversed->ids[4] == 0) {
    // if no additional is linked we return here.
    return path_ids;
  }
  if ((disk_reversed->ids[4] * 50) > additional_size) {
    log::error("Index: path_ids_from_word_id: to search word id would be at "
               "nonexisting location. Index most likely corrupt. Exiting");
  }
  AdditionalBlock *disk_additional = reinterpret_cast<AdditionalBlock *>(
      &mmap_additional[(disk_reversed->ids[4] - 1) * 50]);

  // load the current additional block. -1 because additional IDs start at 1.
  int i = 0;
  size_t current_additional = disk_reversed->ids[4];
  while (true) { // it will break when no new additional is linked
    if (i == 24) {
      if (disk_additional->ids[24] == 0) {
        // no additionals are left.
        break;
      } else {
        // load the new additional block
        current_additional = disk_additional->ids[24];
        if ((current_additional * 50) > additional_size) {
          log::error(
              "Index: path_ids_from_word_id: to search word id would be at "
              "nonexisting location. Index most likely corrupt. Exiting");
        }
        disk_additional = reinterpret_cast<AdditionalBlock *>(
            &mmap_additional[(current_additional - 1) * 50]);
        i = 0;
      }
    }
    if (disk_additional->ids[i] != 0) {
      // save path id
      path_ids.push_back(disk_additional->ids[i]);
    }

    ++i;
  }

  // found all ids.
  return path_ids;
}

// Searches the word list for all words in search words and returns a list of
// path ids and the count of how many appeared. exact_match: if enabled it not
// count words that are the same as the query but different length. e.g query:
// ap and it encounters apple. If false it would count that as a match.
// min_char_for_match will also count words if the first x chars are the same,
// only if exact_match is false.
std::vector<search_path_ids_return>
Index::search_word_list(std::vector<std::string> &search_words,
                        bool exact_match, int min_char_for_match) {
  if (is_mapped == false) {
    map();
  }
  // sort first so we can do char for char compare.
  std::sort(search_words.begin(), search_words.end());

  std::vector<uint64_t> result_word_ids;

  // copy words_f into memory
  std::vector<WordsFValue> words_f(26);
  for (int i = 0; i < 26; ++i) {
    std::memcpy(&words_f[i].bytes[0], &mmap_words_f[i * 12], 12);
  }

  // local index words needs to have atleast 1 value. Is checked by LocalIndex
  // add_to_disk.
  size_t on_disk_count = 0;
  size_t on_disk_id = 0;
  size_t local_word_count = 0;
  size_t local_word_length = search_words[0].length();
  char disk_first_char = 'a';
  char local_first_char = search_words[0][0];

  // check each word on disk. if it is different first letter we will skip using
  // words_f. we will compare chars to chars until they differ. we then know if
  // it is coming first or we are passed it and need to insert the word or we
  // are at the same word.

  while (on_disk_count < words_size) {
    // If the current word first char is different we use words_f to set the
    // location to the start of that char.
    if (disk_first_char < local_first_char) {
      if (words_f[local_first_char - 'a'].location < words_size) {
        log::write(1, "Index: Search: Skipping using Words_f Table");
        on_disk_count = words_f[local_first_char - 'a'].location;
        on_disk_id = words_f[local_first_char - 'a'].id;
        disk_first_char = local_first_char;
      } else {
        if (words_f[local_first_char - 'a'].location == words_size) {
          // Words_f can have entries when if it is the same as words_size it
          // means there are no words for this letter and no subsequent letters
          // too
          break;
        }
        // This should not happen. Index is corrupted.
        log::error("Index: Search: Words_f char value is higher than words "
                   "index size. This means the index is corrupted. Reset the "
                   "index and report this problem.");
      }
    }

    // read the one byte word sperator.
    uint8_t word_seperator = mmap_words[on_disk_count];
    uint32_t word_disk_seperator = 30;

    if (word_seperator < 31 ||
        (word_seperator - 29) + on_disk_count >
            words_size + 1) { // 0-30 is reserved. if it is higher it is for
      // seperator. If the seperator here is 0-30 the index
      // is corrupted.
      log::error(
          "Index: Search: word seperator is invalid. This means the index is "
          "most likely corrupted. Stopping to protect the index.");
    }
    if (disk_first_char - 'a' <
        mmap_words[on_disk_count + 1]) { // + 1 because of the word seperator
      disk_first_char = mmap_words[on_disk_count + 1] + 'a';
    }
    if (word_seperator ==
        255) { // This means the word is larger than 255 bytes. We need to count
               // it manually until we reach another 30< byte.
      // go through start position until end position comparing each char until
      // either it is smaller or bigger. then just try to find the next
      // seperator.
      for (int i = 1; mmap_words[on_disk_count + i] < 31; ++i) {
        ++word_disk_seperator;
        if (words_size <= on_disk_count + i) {
          log::error("Index: Search: Index ends before the next word "
                     "seperator appeared.");
        }
        if (word_seperator < 255) {
          log::error("Index: Search: Word seperator is smaller then the "
                     "expected 255+. Index most likely corrupt.");
        }
      }
    } else {
      word_disk_seperator = word_seperator;
    }

    for (int i = 0; i < word_seperator - 30; ++i) {

      if ((int)mmap_words[on_disk_count + 1 + i] ==
          (int)(search_words[local_word_count][i] - 'a')) {
        // If its last char and words are the same length we found it.
        if (i == local_word_length - 1 &&
            word_seperator - 30 == local_word_length) {
          result_word_ids.push_back(on_disk_id);
          ++local_word_count;
          if (local_word_count ==
              search_words.size()) { // if not more words to compare quit.
            break;
          }
          local_word_length = search_words[local_word_count].length();
          on_disk_count +=
              word_seperator -
              29; // 29 because its length of word + then the next seperator
          ++on_disk_id;
          local_first_char = search_words[local_word_count][0];

          break;
        }

        if (i == local_word_length - 1) {
          // IF exact_match is true check if I is bigger or same than
          // min_char_for_match
          if (!exact_match && i >= min_char_for_match) {
            // still a match
            result_word_ids.push_back(on_disk_id);
          }
          ++local_word_count;
          if (local_word_count ==
              search_words.size()) { // if not more words to compare quit.
            break;
          }
          local_word_length = search_words[local_word_count].length();
          local_first_char = search_words[local_word_count][0];
          break;
        }

        // If its the last on disk char and at the end and not the same
        // length. means we need to skip this word.
        if (i == word_seperator - 31) {
          if (!exact_match) {
            result_word_ids.push_back(on_disk_id);
          }
          on_disk_count +=
              word_seperator -
              29; // 29 because its length of word + then the next seperator
          ++on_disk_id;
          break;
        }
      }

      // If disk char > local char
      if ((int)mmap_words[on_disk_count + 1 + i] >
          (int)(search_words[local_word_count][i] - 'a')) {
        if (!exact_match && i >= min_char_for_match)
          ++local_word_count;
        if (local_word_count ==
            search_words.size()) { // if not more words to compare quit.
          break;
        }
        local_word_length = search_words[local_word_count].length();
        local_first_char = search_words[local_word_count][0];
        break;
      }

      // If disk char < local char
      if ((int)mmap_words[on_disk_count + 1 + i] <
          (int)(search_words[local_word_count][i] - 'a')) {
        if (!exact_match && i >= min_char_for_match)
          ++local_word_count;
        on_disk_count +=
            word_seperator -
            29; // 29 because its length of word + then the next seperator
        ++on_disk_id;
        break;
      }
    }
    if (local_word_count ==
        search_words.size()) { // if not more words to compare quit.
      break;
    }
  }

  // Now we need to read all reversed and additionals and put it into a list of
  // path_id count.
  std::vector<search_path_ids_return> results;
  // we will later combine them but it's easier like this.
  std::vector<uint64_t> path_ids;
  std::vector<uint32_t> counts;
  if (result_word_ids.size() == 0)
    return results;
  for (size_t i = 0; i < result_word_ids.size(); ++i) {
    for (const int &path_id : path_ids_from_word_id(result_word_ids[i])) {
      // potential future performance improvments
      if (auto it = std::find(path_ids.begin(), path_ids.end(), path_id);
          it == path_ids.end()) {
        path_ids.push_back(path_id);
        counts.push_back(1);
      } else {
        ++counts[it - path_ids.begin()]; // increase count of the element
      }
    }
  }
  // convert into struct and return.
  results.reserve(path_ids.size());
  for (size_t i = 0; i < path_ids.size(); ++i) {
    results.push_back({path_ids[i], counts[i]});
  }
  return results;
}

// return a unordered map of ID and path string.
std::unordered_map<uint64_t, std::string>
Index::id_to_path_string(std::vector<search_path_ids_return> path_ids) {
  if (is_mapped == false) {
    map();
  }
  std::unordered_map<uint64_t, std::string> results;
  results.reserve(path_ids.size());

  std::vector<search_path_ids_return> sorted_path_ids = path_ids;
  std::sort(sorted_path_ids.begin(), sorted_path_ids.end());
  uint64_t path_count = 1; // on-disk path ids are indexed from 1.
  uint64_t local_count = 0;
  if (paths_size < 2) { // shouldn't happen. invalid. index corrupt.
    log::error("Index: Search: id_to_path_string: Error. Paths index invalid. "
               "Index most likely corrupt.");
  }
  for (size_t i = 0; i < paths_size;) {
    PathOffset *path_length = reinterpret_cast<PathOffset *>(
        &mmap_paths[i]); // load the length offset
    i += 2;

    if (path_count == sorted_path_ids[local_count].path_id) {
      if (paths_size < i + path_length->offset) { // shouldn't happen.
                                                  // invalid. index corrupt.
        log::error(
            "Index: Search: id_to_path_string: Error. Paths index invalid. "
            "Index most likely corrupt.");
      }
      results[sorted_path_ids[local_count].path_id] =
          std::string(&mmap_paths[i], path_length->offset);
      ++local_count;
    }
    ++path_count;
    if (local_count == path_ids.size()) {
      break;
    }
    i += path_length->offset;
  }
  return results;
}

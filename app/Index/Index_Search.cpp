#include "../Logging/logging.h"
#include "index.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

std::vector<PATH_ID_TYPE> Index::path_ids_from_word_id(uint64_t word_id) {
  std::vector<PATH_ID_TYPE> path_ids;
  if ((word_id * REVERSED_ENTRY_SIZE) + REVERSED_ENTRY_SIZE > reversed_size) {
    Log::error("Index: path_ids_from_word_id: to search word id would be at "
               "nonexisting location. Index most likely corrupt. Exiting");
  }
  // load the reversed block into memory.
  ReversedBlock *disk_reversed = reinterpret_cast<ReversedBlock *>(
      &mmap_reversed[word_id * REVERSED_ENTRY_SIZE]);
  for (int i = 0; i < REVERSED_PATH_LINKS_AMOUNT; ++i) {
    if (disk_reversed->ids.path[i] != 0) {
      path_ids.push_back(disk_reversed->ids.path[i]);
    }
  }

  if (disk_reversed->ids.additional[0] == 0) {
    // if no additional is linked we return here.
    return path_ids;
  }
  if ((disk_reversed->ids.additional[0] * ADDITIONAL_ENTRY_SIZE) >
      additional_size) {
    Log::error("Index: path_ids_from_word_id: Additional block would be at non "
               "existing location. Exiting");
  }
  AdditionalBlock *disk_additional = reinterpret_cast<AdditionalBlock *>(
      &mmap_additional[(disk_reversed->ids.additional[0] - 1) *
                       ADDITIONAL_ENTRY_SIZE]);

  // load the current additional block. -1 because additional IDs start at 1.
  int i = 0;
  size_t current_additional = disk_reversed->ids.additional[0];
  while (true) { // it will break when no new additional is linked
    if (i == ADDITIONAL_PATH_LINKS_AMOUNT) {
      if (disk_additional->ids.additional[0] == 0) {
        // no additionals are left.
        break;
      } else {
        // load the new additional block
        current_additional = disk_additional->ids.additional[0];
        if ((current_additional * ADDITIONAL_ENTRY_SIZE) > additional_size) {
          Log::error("Index: path_ids_from_word_id: Additional block would be "
                     "at non existing location. Exiting");
        }
        disk_additional = reinterpret_cast<AdditionalBlock *>(
            &mmap_additional[(current_additional - 1) * ADDITIONAL_ENTRY_SIZE]);
        i = 0;
      }
    }
    if (disk_additional->ids.path[i] != 0) {
      // save path id
      path_ids.push_back(disk_additional->ids.path[i]);
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
                        std::vector<bool> exact_match, int min_char_for_match) {
  std::vector<std::vector<uint64_t>> result_word_ids(
      search_words
          .size()); // because of exact match some words may have multiple
                    // word ids connected. and to keep it connected to the
                    // search words for later processing we need to do this.
  std::vector<search_path_ids_return> results;
  if (!initialized || !health_status()) {
    Log::write(3, "Index is not initialized or it's not readable(e.g because a "
                  "Transaction Execution is in progress)");
    return results;
  }
  if (is_mapped == false) {
    map();
  }

  if (search_words.size() == 0) {
    return results;
  }
  // sort first so we can do char for char compare. This should be done already
  // in the search function but we will do it again because this is public and
  // if not sorted it may freeze.
  std::sort(search_words.begin(), search_words.end());

  // copy words_f into memory
  std::vector<WordsFValue> words_f(26);
  for (int i = 0; i < 26; ++i) {
    std::memcpy(&words_f[i].bytes[0],
                &mmap_words_f[i * (8 + WORDS_F_LOCATION_SIZE)],
                (8 + WORDS_F_LOCATION_SIZE));
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
        Log::write(1, "Index: Search: Skipping using Words_f Table");
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
        Log::error("Index: Search: Words_f char value is higher than words "
                   "index size. This means the index is corrupted. Reset the "
                   "index and report this problem.");
      }
    }

    // read the one byte word sperator.
    WordSeperator word_sep;
    for (uint8_t i = 0; i < WORD_SEPARATOR_SIZE; ++i) {
      word_sep.bytes[i] = mmap_words[on_disk_count + i];
    }
    WORD_SEPARATOR_TYPE word_seperator = word_sep.seperator;
    if (word_seperator <= 0) {
      // can't be 0 or lower than 0. index corrupt most likely
      Log::error("Index: Combine: Word Seperator is 0 or lower. This can not "
                 "be. Index most likely corrupt.");
    }

    if (disk_first_char <
        mmap_words[on_disk_count +
                   WORD_SEPARATOR_SIZE]) { // + 1 because of the word seperator
      disk_first_char = mmap_words[on_disk_count + WORD_SEPARATOR_SIZE];
    }

    for (int i = 0; i < word_seperator; ++i) {

      if ((int)mmap_words[on_disk_count + WORD_SEPARATOR_SIZE + i] ==
          (int)(search_words[local_word_count][i])) {

        // If its last char and words are the same length we found it.
        if (i == local_word_length - 1 && word_seperator == local_word_length) {
          result_word_ids[local_word_length].push_back(on_disk_id);
          ++local_word_count;
          if (local_word_count ==
              search_words.size()) { // if not more words to compare quit.
            break;
          }
          local_word_length = search_words[local_word_count].length();
          on_disk_count +=
              word_seperator + WORD_SEPARATOR_SIZE; // then the next seperator
          ++on_disk_id;
          local_first_char = search_words[local_word_count][0];

          break;
        }

        if (i == local_word_length - 1) {
          // IF exact_match is true check if I is bigger or same than
          // min_char_for_match
          if (!exact_match[local_word_count] && i >= min_char_for_match) {
            // still a match
            result_word_ids[local_word_count].push_back(on_disk_id);
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
        if (i == word_seperator - 1) {
          if (!exact_match[local_word_count] && i >= min_char_for_match) {
            result_word_ids[local_word_count].push_back(on_disk_id);
          }
          on_disk_count +=
              word_seperator + WORD_SEPARATOR_SIZE; // then the next seperator
          ++on_disk_id;
          break;
        }
      }

      // If disk char > local char
      if ((int)mmap_words[on_disk_count + WORD_SEPARATOR_SIZE + i] >
          (int)(search_words[local_word_count][i])) {
        if (!exact_match[local_word_count] && i >= min_char_for_match) {
          result_word_ids[local_word_count].push_back(on_disk_id);
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

      // If disk char < local char
      if ((int)mmap_words[on_disk_count + WORD_SEPARATOR_SIZE + i] <
          (int)(search_words[local_word_count][i])) {
        if (!exact_match[local_word_count] && i >= min_char_for_match) {
          result_word_ids[local_word_count].push_back(on_disk_id);
        }
        on_disk_count += word_seperator + 1; // then the next seperator
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
  // we will later combine them but it's easier like this.
  std::vector<PATH_ID_TYPE> path_ids;
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
std::unordered_map<PATH_ID_TYPE, std::string>
Index::id_to_path_string(std::vector<search_path_ids_return> path_ids) {
  std::unordered_map<PATH_ID_TYPE, std::string> results;
  if (!initialized || !health_status()) {
    Log::write(3, "Index is not initialized or it's not readable(e.g because a "
                  "Transaction Execution is in progress)");
    return results;
  }
  if (is_mapped == false) {
    map();
  }

  if (path_ids.size() == 0) {
    return results;
  }
  results.reserve(path_ids.size());

  std::vector<search_path_ids_return> sorted_path_ids = path_ids;
  std::sort(sorted_path_ids.begin(), sorted_path_ids.end());
  uint64_t path_count = 1; // on-disk path ids are indexed from 1.
  uint64_t local_count = 0;
  if (paths_size < 2) { // shouldn't happen. invalid. index corrupt.
    Log::error("Index: Search: id_to_path_string: Error. Paths index invalid. "
               "Index most likely corrupt.");
  }
  for (size_t i = 0; i < paths_size;) {
    PathOffset *path_length = reinterpret_cast<PathOffset *>(
        &mmap_paths[i]); // load the length offset
    i += 2;

    if (path_count == sorted_path_ids[local_count].path_id) {
      if (paths_size < i + path_length->offset) { // shouldn't happen.
                                                  // invalid. index corrupt.
        Log::error(
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

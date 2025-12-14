#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// expensive index check. returns true if index is correct. returns false if index is corrupt or could not check it.
bool Index::expensive_index_check(const bool verbose_output)
{
  if (!initialized || !health_status()) {
    Log::write(3, "Index is not initialized or it's not readable(e.g because a "
               "Transaction Execution is in progress)");
    if (verbose_output) {
      std::cout
        << "\nIndex is not initialized or it's not readable(e.g because a "
        "Transaction Execution is in progress)\n";
    }
    return false;
  }
  if (is_mapped == false) {
    map();
  }

  // Acquire Lock (even though we don't write anything we don't want to the
  // check to potentially output wrong information if another process updates
  // the index.
  if (!Index::lock(false)) {
    Log::write(3, "Index could not be locked. There is probably another process currently using the Index.");
    if (verbose_output) {
      std::cout
        << "\nIndex could not be locked. There is probably another process currently using the Index.\n";
    }
    return false;
  }

  // Check words Index:
  // 1. Validate that the seperator is correct
  // 2. Validate that non-seperators are only a-z
  // 3. Also build a words_f so we can compare it later with words_f.
  // 4. Keep track of other values like size, count for later use.
  uint64_t words_check_size = 0;
  WORDS_F_LOCATION_TYPE words_check_count = 0;
  std::vector<WordsFValue> words_f_comparison(26);

  char disk_first_char = 'a';
  std::string previous_word = "";

  while (words_check_size < words_size) {
    // read the one byte word separator.
    WordSeperator word_sep;
    for (uint8_t i = 0; i < WORD_SEPARATOR_SIZE; ++i) {
      word_sep.bytes[i] = mmap_words[words_check_size + i];
    }
    WORD_SEPARATOR_TYPE word_seperator = word_sep.seperator;
    if (word_seperator <= 0) {
      // can't be 0 or lower than 0. index corrupt most likely
      Log::write(4, "Index: Check: Word Seperator is 0 or lower. This can not "
                 "be. Index most likely corrupt.");
      if (verbose_output) {
        std::cout
          << "\nCheck: Words Index: Word seperator is lower or equal to 0 which is invalid. Index most likely corrupt.\n";
      }
      return false;
    }

    if (words_check_size + word_seperator >= words_size) {
      Log::write(4, "Index: Check: Word Seperator would go over words index.");
      if (verbose_output) {
        std::cout
          << "\nCheck: Words Index: Word Seperator would go over words index. Index most likely corrupt.\n";
      }
      return false;
    }

    // check if new words_f
    if (disk_first_char <
      mmap_words[words_check_size +
        WORD_SEPARATOR_SIZE]) {
      // + WORD_SEPARATOR_SIZE because of the word separator
      disk_first_char = mmap_words[words_check_size + WORD_SEPARATOR_SIZE];
      words_f_comparison[disk_first_char - 'a'] = {
        .location = words_check_size + WORD_SEPARATOR_SIZE, .id = words_check_count
      };
    }

    std::string current_word = "";
    for (int i = 0; i < word_seperator; ++i) {
      char c = mmap_words[words_check_size + WORD_SEPARATOR_SIZE + i];
      if (c < 'a' || c > 'z') {
        // Invalid. TODO: allow customising the range
        Log::write(4, "Index: Check: Char is not a-z. Invalid for a word char.");
        if (verbose_output) {
          std::cout
            << "\nCheck: Words Index: Char is not a-z. Invalid for a word char. Index most likely corrupt.\n";
        }
        return false;
      }
      current_word += c;
    }
    // check that previous word is smaller than current word and verify it's not the same
    if (current_word <= previous_word) {
      Log::write(4, "Index: Check: Word is smaller than previous word or the same.");
      if (verbose_output) {
        std::cout
          << "\nCheck: Words Index: Word is smaller than previous word or the same. Index most likely corrupt.\n";
      }
      return false;
    }
    previous_word = current_word;
    ++words_check_count;

    words_check_size +=
      word_seperator + WORD_SEPARATOR_SIZE;
  }
  if (words_check_size != words_size) {
    Log::write(4, "Index: Check: Word size does not match up with counted size.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Words Index: Index: Check: Word size does not match up with counted size.\n";
    }
    return false;
  }

  // Check Words_f Index:
  // 1. Validate size
  // 2. Compare values to previously created one (if our is 0 just validate it is the same as the next non zero)

  if (words_f_size != (8 + WORDS_F_LOCATION_SIZE) * 26) {
    Log::write(4, "Index: Check: Words_f size is wrong.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Words Index: Index: Check: Words_f size is wrong.\n";
    }
    return false;
  }

  // copy words_f into memory
  std::vector<WordsFValue> words_f(26);
  for (int i = 0; i < 26; ++i) {
    std::memcpy(&words_f[i].bytes[0],
                &mmap_words_f[i * (8 + WORDS_F_LOCATION_SIZE)],
                (8 + WORDS_F_LOCATION_SIZE));
  }

  if (words_f[0].id != 0 || words_f[0].location != 0) {
    Log::write(4, "Index: Check: Word_f: location and/or id for letter a is not 0 which is invalid.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Word_f: location and/or id for letter a is not 0 which is invalid.\n";
    }
    return false;
  }

  // compare them
  for (int i = 1; i < 26; ++i) {
    if (words_f_comparison[i].location == words_f[i].location && words_f_comparison[i].id == words_f[i].id) {
      continue;
    }
    if (words_f[i].location == 0 || words_f[i].id == 0) {
      Log::write(4, "Index: Check: Word_f: location and/or id is 0 which is not allowed.");
      if (verbose_output) {
        std::cout
          << "\nCheck: Word_f: location and/or id is 0 which is not allowed.\n";
      }
      return false;
    }
    // if local is zero, get the next non-zero value and compare.
    if (words_f_comparison[i].id == 0 || words_f_comparison[i].location == 0) {
      WordsFValue next_value = {.location = 0, .id = 0};
      for (int j = i + 1; j < 26; ++j) {
        if (words_f_comparison[j].id != 0) {
          next_value = {.location = words_f_comparison[j].location, .id = words_f_comparison[j].id};
          break;
        }
      }
      if (next_value.location == 0) {
        next_value = {.location = words_check_size, .id = words_check_count};
      }
      if (words_f[i].location != next_value.location || words_f[i].id != next_value.id) {
        Log::write(4, "Index: Check: Word_f: location and/or id is wrong for values that use the next-non value.");
        if (verbose_output) {
          std::cout
            << "\nCheck: Word_f: location and/or id is wrong for values that use the next-non value.\n";
        }
        return false;
      }
    }
  }

  // Paths Index check
  // 1. make sure seperators are correct and size matches
  // 2. keep track of counts

  uint64_t paths_check_size = 0;
  uint64_t paths_check_count = 0;
  uint16_t next_path_end = 0; // if 0 the next 2 values are the header.

  while (paths_check_size < paths_size) {
    if (paths_check_size + 1 < paths_size) {
      // we read 1 byte ahead for the offset to prevent
      // accessing invalid data. The index format would allow it
      // but it could be corrupted and not detected.
      PathOffset path_offset;
      // we read the offset so we know how long the path is and where the next
      // path starts.
      path_offset.bytes[0] = mmap_paths[paths_check_size];
      ++paths_check_size;
      path_offset.bytes[1] = mmap_paths[paths_check_size];
      next_path_end = path_offset.offset;
      ++paths_check_size;
      if (!(paths_check_size + next_path_end <
        paths_size + 1)) {
        Log::write(4, "Index: Check: Paths index too small to read next path.");
        if (verbose_output) {
          std::cout
            << "\nCheck: Paths index too small to read next path.\n";
        }
        return false;
      }
      paths_check_size += next_path_end;
      ++paths_check_count;
    }
    else {
      Log::write(4, "Index: Check: Paths index too small to read separator.");
      if (verbose_output) {
        std::cout
          << "\nCheck: Paths index too small to read separator.\n";
      }
      return false;
    }
  }

  // Paths count
  // 1. Just make sure the size is 4 * amount of paths

  if (paths_count_size != paths_check_count * 4) {
    Log::write(4, "Index: Check: Paths count size is not valid.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Paths count size is not valid.\n";
    }
    return false;
  }

  // reversed & Additional
  // 1. Check that reversed is dividable by 10
  // 2. Check that additional is dividable by 50
  // 3. Read reversed, make sure no duplicate path ids are found and all path ids are valid. Also make sure there are no gaps
  // 4. Read additional and make sure it's valid. Keep track of all additionals. Do the same for each path id in additional as in reversed and follow all additionals

  if (reversed_size % REVERSED_ENTRY_SIZE != 0 ||
    additional_size
    % ADDITIONAL_ENTRY_SIZE != 0) {
    Log::write(4, "Index: Check: Reversed and/or Additional size is invalid.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Reversed and/or Additional size is invalid.\n";
    }
    return false;
  }
  if (reversed_size != words_check_count * REVERSED_ENTRY_SIZE) {
    Log::write(4, "Index: Check: Reversed size does not match up with amounts of words.");
    if (verbose_output) {
      std::cout
        << "\nCheck: Reversed size does not match up with amounts of words.\n";
    }
    return false;
  }

  uint64_t reversed_check_count = 0;
  uint64_t reversed_check_size = 0;
  std::unordered_set<ADDITIONAL_ID_TYPE> used_additional_ids;


  while (reversed_check_count < words_check_count) {
    std::unordered_set<PATH_ID_TYPE> used_path_ids;
    ADDITIONAL_ID_TYPE current_additional = 0;

    bool found_path = false;
    bool found_emtpy = false;

    ReversedBlock* disk_reversed = reinterpret_cast<ReversedBlock*>(
      &mmap_reversed[reversed_check_count * REVERSED_ENTRY_SIZE]);

    for (PATH_ID_TYPE i = 0; i < REVERSED_PATH_LINKS_AMOUNT;
         ++i) {
      if (disk_reversed->ids.path[i] == 0) {
        found_emtpy = true;
      }
      else {
        if (!found_path) { found_path = true; }
        if (found_emtpy) {
          Log::write(4, "Index: Check: Reversed entry has a 0 in between which should not happen.");
          if (verbose_output) {
            std::cout
              << "\nCheck: Reversed entry has a 0 in between which should not happen.\n";
          }
          return false;
        }
        if (disk_reversed->ids.path[i] > paths_check_count) {
          Log::write(4, "Index: Check: Reversed entry has a too large path id.");
          if (verbose_output) {
            std::cout
              << "\nCheck: Reversed entry has a too large path id.\n";
          }
          return false;
        }
        if (used_path_ids.count(disk_reversed->ids.path[i]) != 0) {
          Log::write(4, "Index: Check: Reversed entry has a duplicate path id.");
          if (verbose_output) {
            std::cout
              << "\nCheck: Reversed entry has a duplicate path id.\n";
          }
          return false;
        }
      }
      used_path_ids.insert(disk_reversed->ids.path[i]);
    }

    // now check additionals
    current_additional = disk_reversed->ids.additional[0];

    while (current_additional != 0) {
      if (found_emtpy && current_additional != 0) {
        Log::write(4, "Index: Check: There is an additional linked even though a gap was found in reversed.");
        if (verbose_output) {
          std::cout
            << "\nCheck: There is an additional linked even though a gap was found in reversed.\n";
        }
        return false;
      }
      if ((current_additional * ADDITIONAL_ENTRY_SIZE) > additional_size) {
        Log::write(4, "Index: Check: Additional linked id is invalid.");
        if (verbose_output) {
          std::cout
            << "\nCheck: Additional linked id is invalid.\n";
        }
        return false;
      }
      if (used_additional_ids.count(current_additional) != 0) {
        Log::write(4, "Index: Check: Additional block linked more than once.");
        if (verbose_output) {
          std::cout
            << "\nCheck: Additional block linked more than once.\n";
        }
        return false;
      }

      used_additional_ids.insert(current_additional);

      AdditionalBlock* disk_additional = reinterpret_cast<AdditionalBlock*>(
        &mmap_additional[(current_additional - 1) * ADDITIONAL_ENTRY_SIZE]);

      for (PATH_ID_TYPE i = 0; i < ADDITIONAL_PATH_LINKS_AMOUNT;
           ++i) {
        if (disk_additional->ids.path[i] == 0) {
          found_emtpy = true;
        }
        else {
          if (!found_path) { found_path = true; }
          if (found_emtpy) {
            Log::write(4, "Index: Check: Additional entry has a 0 in between which should not happen.");
            if (verbose_output) {
              std::cout
                << "\nCheck: Additional entry has a 0 in between which should not happen.\n";
            }
            return false;
          }
          if (disk_additional->ids.path[i] > paths_check_count) {
            Log::write(4, "Index: Check: Additional entry has a too large path id.");
            if (verbose_output) {
              std::cout
                << "\nCheck: Additional entry has a too large path id.\n";
            }
            return false;
          }
          if (used_path_ids.count(disk_additional->ids.path[i]) != 0) {
            Log::write(4, "Index: Check: Additional entry has a duplicate path id.");
            if (verbose_output) {
              std::cout
                << "\nCheck: Additional entry has a duplicate path id.\n";
            }
            return false;
          }
        }
        used_path_ids.insert(disk_reversed->ids.path[i]);
      }
      current_additional = disk_additional->ids.additional[0];
    }

    ++reversed_check_count;
  }


  Index::unlock(false);
  Log::write(2, "Index: Check: No corruption detected.");
  if (verbose_output) {
    std::cout
      << "\nCheck: Index seems to be correct. No corruption or any sign of corruption detected.\n";
  }
  return true;
}


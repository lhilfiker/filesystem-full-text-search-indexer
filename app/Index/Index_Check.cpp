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
  size_t words_check_size = 0;
  WORDS_F_LOCATION_TYPE words_check_count = 0;
  std::vector<WordsFValue> words_f_comparison(26);

  char disk_first_char = 'a';
  std::string previous_word = "";

  while (words_check_size < words_size) {
    // read the one byte word sperator.
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
      // + WORD_SEPARATOR_SIZE because of the word seperator
      disk_first_char = mmap_words[words_check_size + WORD_SEPARATOR_SIZE];
      words_f_comparison[disk_first_char] = {.location = words_check_size, .id = words_check_count};
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


  Index::unlock(false);
  Log::write(2, "Index: Check: No corruption detected.");
  if (verbose_output) {
    std::cout
      << "\nCheck: Index seems to be correct. No corruption or any sign of corruption detected.\n";
  }
  return true;
}

#include "../index.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

WORD_SEPARATOR_TYPE Index::DiskIO::get_word_separator(const size_t start_pos) const
{
#ifndef NDEBUG
  if (words_size < start_pos + WORD_SEPARATOR_SIZE) {
    Log::error("Index::DiskIO::get_word_separator: out of bounds access denied.");
  }
#endif
  WORD_SEPARATOR_TYPE separator;
  std::memcpy(&separator
              ,
              &
              mmap_words[start_pos],
              sizeof
              (WORD_SEPARATOR_TYPE)
  );
  return
    separator;
}

void Index::DiskIO::set_word_separator(const size_t start_pos, const WORD_SEPARATOR_TYPE separator)
{
#ifndef NDEBUG
  if (words_size < start_pos + WORD_SEPARATOR_SIZE) {
    Log::error("Index::DiskIO::set_word_separator: out of bounds access denied.");
  }
#endif
  std::memcpy(&mmap_words[start_pos], &separator, sizeof(WORD_SEPARATOR_TYPE));
}

char Index::DiskIO::get_char_of_word(size_t word_start_pos, uint16_t char_pos) const
{
#ifndef NDEBUG
  if (words_size < word_start_pos + WORD_SEPARATOR_SIZE + char_pos) {
    Log::error("Index::DiskIO::get_char_of_word: out of bounds access denied.");
  }
#endif
  return mmap_words[word_start_pos +
    WORD_SEPARATOR_SIZE + char_pos];
}

void Index::DiskIO::set_word(const size_t start_pos, const std::string& word, const uint16_t length)
{
#ifndef NDEBUG
  if (words_size < start_pos + length) {
    Log::error("Index::DiskIO::set_word: out of bounds access denied.");
  }
#endif
  std::memcpy(&mmap_words[start_pos], word.data(), length);
}

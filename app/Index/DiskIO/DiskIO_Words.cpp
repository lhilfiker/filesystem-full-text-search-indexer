#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

WORD_SEPARATOR_TYPE Index::DiskIO::get_word_separator(const size_t start_pos) const
{
  return *reinterpret_cast<const WORD_SEPARATOR_TYPE*>(&mmap_words[start_pos]);
}

char Index::DiskIO::get_char_of_word(size_t word_start_pos, int char_pos) const
{
  return mmap_words[word_start_pos +
    WORD_SEPARATOR_SIZE + char_pos];
}

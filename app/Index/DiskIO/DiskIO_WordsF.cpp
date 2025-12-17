#include "../index.h"
#include "../../Logging/logging.h"
#include <cstring>

std::vector<WordsFValue> Index::DiskIO::get_words_f() const
{
#ifndef NDEBUG
  if (words_f_size < 26 * sizeof(WordsFValue)) {
    Log::error("Index::DiskIO::get_words_f: out of bounds access denied.");
  }
#endif
  std::vector<WordsFValue> words_f(26);
  std::memcpy(words_f.data(), mmap_words_f.data(), 26 * sizeof(WordsFValue));
  return words_f;
}

void Index::DiskIO::set_words_f(std::vector<WordsFValue>& words_f)
{
#ifndef NDEBUG
  if (words_f_size < 26 * sizeof(WordsFValue)) {
    Log::error("Index::DiskIO::set_words_f: out of bounds access denied.");
  }
#endif
  std::memcpy(&mmap_words_f[0], words_f.data(), 26 * sizeof(WordsFValue));
}

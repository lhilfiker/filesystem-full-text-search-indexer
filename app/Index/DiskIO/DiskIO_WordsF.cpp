#include "../index.h"
#include <cstring>

std::vector<WordsFValue> Index::DiskIO::get_words_f() const
{
  std::vector<WordsFValue> words_f(26);
  std::memcpy(words_f.data(), mmap_words_f.data(), 26 * sizeof(WordsFValue));
  return words_f;
}

void Index::DiskIO::set_words_f(std::vector<WordsFValue>& words_f)
{
  std::memcpy(&mmap_words_f[0], words_f.data(), 26 * sizeof(WordsFValue));
}

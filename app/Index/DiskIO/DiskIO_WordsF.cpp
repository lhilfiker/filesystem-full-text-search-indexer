#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

std::vector<WordsFValue> Index::DiskIO::get_words_f() const
{
  std::vector<WordsFValue> words_f(26);
  std::memcpy(words_f.data(), mmap_words_f.data(), 26 * sizeof(WordsFValue));
  return words_f;
}

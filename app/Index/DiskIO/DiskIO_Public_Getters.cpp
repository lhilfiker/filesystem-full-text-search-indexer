#include "../index.h"
#include <fstream>


size_t Index::DiskIO::get_paths_size() const
{
  return paths_size;
}

size_t Index::DiskIO::get_paths_count_size() const
{
  return paths_count_size;
}

size_t Index::DiskIO::get_words_size() const
{
  return words_size;
}

size_t Index::DiskIO::get_words_f_size() const
{
  return words_f_size;
}

size_t Index::DiskIO::get_reversed_size() const
{
  return reversed_size;
}

size_t Index::DiskIO::get_additional_size() const
{
  return additional_size;
}

bool Index::DiskIO::get_mapped() const
{
  return is_mapped;
}

bool Index::DiskIO::get_locked() const
{
  return is_locked;
}

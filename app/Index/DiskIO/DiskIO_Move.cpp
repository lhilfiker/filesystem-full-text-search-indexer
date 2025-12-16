#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "CliParser/cliparser.h"
#include "CliParser/cliparser.h"
// index types:
// paths = 0, words = 1, words_f = 2, reversed = 3,
// additional = 4, paths_count = 5
mio::mmap_sink* Index::DiskIO::get_target_mmap(uint8_t target_index)
{
  switch (target_index) {
  case 1:
    return &mmap_words;
  case 2:
    return &mmap_words_f;
  case 3:
    return &mmap_reversed;
  case 4:
    return &mmap_additional;
  case 5:
    return &mmap_paths_count;
  default:
    return &mmap_paths;
  }
}

void Index::DiskIO::copy_from_index(uint8_t source_index, mio::mmap_sink& target, size_t target_pos, size_t source_pos,
                                    size_t length)
{
  std::memcpy(&target[target_pos], &(*get_target_mmap(source_index))[source_pos], length);
}


void Index::DiskIO::copy_to_index(uint8_t target_index, mio::mmap_sink& source, size_t target_pos, size_t source_pos,
                                  size_t length)
{
  std::memcpy(&(*get_target_mmap(target_index))[target_pos], &source[source_pos], length);
}

void Index::DiskIO::shift_data(uint8_t target_index, size_t target_pos, size_t source_pos,
                               size_t length)
{
  std::memmove(&(*get_target_mmap(target_index))[target_pos], &(*get_target_mmap(target_index))[source_pos], length);
}

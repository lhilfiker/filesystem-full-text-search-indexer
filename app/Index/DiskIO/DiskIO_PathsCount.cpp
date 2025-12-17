#include "../index.h"
#include "../../Logging/logging.h"
#include <filesystem>
#include <fstream>

uint32_t Index::DiskIO::get_path_count(const size_t path_id) const
{
#ifndef NDEBUG
  if (paths_count_size < path_id * 4) {
    Log::error("Index::DiskIO::get_path_count: out of bounds access denied.");
  }
#endif
  return *reinterpret_cast<const uint32_t*>(&mmap_paths_count[(path_id - 1) * 4]);
}

void Index::DiskIO::set_path_count(const size_t path_id, const uint32_t path_count)
{
#ifndef NDEBUG
  if (paths_count_size < path_id * 4) {
    Log::error("Index::DiskIO::set_path_count: out of bounds access denied.");
  }
#endif
  *reinterpret_cast<uint32_t*>(&mmap_paths_count[(path_id - 1) * 4]) = path_count;
}

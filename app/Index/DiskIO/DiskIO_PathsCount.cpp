#include "../index.h"
#include <filesystem>
#include <fstream>

uint32_t Index::DiskIO::get_path_count(const size_t path_id) const
{
  return *reinterpret_cast<const uint32_t*>(&mmap_paths_count[(path_id - 1) * 4]);
}

void Index::DiskIO::set_path_count(const size_t path_id, const uint32_t path_count)
{
  *reinterpret_cast<uint32_t*>(&mmap_paths_count[(path_id - 1) * 4]) = path_count;
}

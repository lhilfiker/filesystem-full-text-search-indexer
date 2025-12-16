#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

uint16_t Index::DiskIO::get_path_separator(const size_t start_pos) const
{
  return *reinterpret_cast<const uint16_t*>(&mmap_paths[start_pos]);
}

void Index::DiskIO::set_path_separator(const size_t start_pos, const uint16_t separator)
{
  *reinterpret_cast<uint16_t*>(&mmap_paths[start_pos]) = separator;
}

// Start pos is the pos of the first path byte and not the path separator.
std::string Index::DiskIO::get_path(const size_t start_pos, const uint16_t length) const
{
  return std::string(&mmap_paths[start_pos], length);
}

void Index::DiskIO::set_path(const size_t start_pos, const std::string& path, const uint16_t length)
{
  std::memcpy(&mmap_paths[start_pos], path.data(), length);
}

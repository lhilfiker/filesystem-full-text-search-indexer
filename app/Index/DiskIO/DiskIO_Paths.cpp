#include "../index.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

uint16_t Index::DiskIO::get_path_separator(const size_t start_pos) const
{
#ifndef NDEBUG
  if (paths_size < start_pos + 2) {
    Log::error("Index::DiskIO::get_path_separator: out of bounds access denied.");
  }
#endif
  uint16_t separator;
  std::memcpy(&separator, &mmap_paths[start_pos], sizeof(uint16_t));
  return separator;
}

void Index::DiskIO::set_path_separator(const size_t start_pos, const uint16_t separator)
{
#ifndef NDEBUG
  if (paths_size < start_pos + 2) {
    Log::error("Index::DiskIO::set_path_separator: out of bounds access denied.");
  }
#endif
  std::memcpy(&mmap_paths[start_pos], &separator, sizeof(uint16_t));
}

// Start pos is the pos of the first path byte and not the path separator.
std::string Index::DiskIO::get_path(const size_t start_pos, const uint16_t length) const
{
#ifndef NDEBUG
  if (paths_size < start_pos + length) {
    Log::error("Index::DiskIO::get_path: out of bounds access denied.");
  }
#endif
  return std::string(&mmap_paths[start_pos], length);
}

void Index::DiskIO::set_path(const size_t start_pos, const std::string& path, const uint16_t length)
{
#ifndef NDEBUG
  if (paths_size < start_pos + length) {
    Log::error("Index::DiskIO::set_path: out of bounds access denied.");
  }
#endif
  std::memcpy(&mmap_paths[start_pos], &path[0], length);
}

#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

uint32_t Index::DiskIO::get_path_count(const size_t path_id) const
{
  return *reinterpret_cast<const uint32_t*>(&mmap_paths_count[(path_id - 1) * 4]);
}

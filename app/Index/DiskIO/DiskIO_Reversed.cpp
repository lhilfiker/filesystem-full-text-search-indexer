#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

ReversedBlock* Index::DiskIO::get_reversed_pointer(const size_t id)
{
  return reinterpret_cast<ReversedBlock*>(
    &mmap_reversed[id * REVERSED_ENTRY_SIZE]);
}

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

void Index::DiskIO::set_reversed(const ReversedBlock& reversed_block, size_t word_id)
{
  *reinterpret_cast<ReversedBlock*>(&mmap_paths_count[word_id * REVERSED_ENTRY_SIZE]) = reversed_block;
}

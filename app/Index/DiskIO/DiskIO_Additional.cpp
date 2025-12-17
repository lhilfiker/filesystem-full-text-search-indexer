#include "../index.h"
#include "../../Logging/logging.h"
#include <filesystem>
#include <fstream>

AdditionalBlock* Index::DiskIO::get_additional_pointer(const ADDITIONAL_ID_TYPE id)
{
#ifndef NDEBUG
  if (additional_size < id * ADDITIONAL_ENTRY_SIZE) {
    Log::error("Index::DiskIO::get_additional_pointer: out of bounds access denied.");
  }
#endif
  return reinterpret_cast<AdditionalBlock*>(
    &mmap_additional[(id - 1) * ADDITIONAL_ENTRY_SIZE]);
}

void Index::DiskIO::set_additional(const AdditionalBlock& additional_block, size_t additional_id)
{
#ifndef NDEBUG
  if (additional_size < additional_id * ADDITIONAL_ENTRY_SIZE) {
    Log::error("Index::DiskIO::get_additional_pointer: out of bounds access denied.");
  }
#endif
  *reinterpret_cast<AdditionalBlock*>(&mmap_additional[(additional_id - 1) * ADDITIONAL_ENTRY_SIZE]) =
    additional_block;
}

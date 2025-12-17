#include "../index.h"
#include <filesystem>
#include <fstream>

AdditionalBlock* Index::DiskIO::get_additional_pointer(const ADDITIONAL_ID_TYPE id)
{
  return reinterpret_cast<AdditionalBlock*>(
    &mmap_additional[(id - 1) * ADDITIONAL_ENTRY_SIZE]);
}

void Index::DiskIO::set_additional(const AdditionalBlock& additional_block, size_t additional_id)
{
  *reinterpret_cast<AdditionalBlock*>(&mmap_additional[(additional_id - 1) * ADDITIONAL_ENTRY_SIZE]) =
    additional_block;
}

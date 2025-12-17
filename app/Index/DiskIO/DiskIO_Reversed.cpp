#include "../index.h"
#include "../../Logging/logging.h"
#include <fstream>

ReversedBlock* Index::DiskIO::get_reversed_pointer(const size_t id)
{
#ifndef NDEBUG
  if (reversed_size < id * REVERSED_ENTRY_SIZE) {
    Log::error("Index::DiskIO::get_reversed_pointer: out of bounds access denied.");
  }
#endif
  return reinterpret_cast<ReversedBlock*>(
    &mmap_reversed[id * REVERSED_ENTRY_SIZE]);
}

void Index::DiskIO::set_reversed(const ReversedBlock& reversed_block, size_t word_id)
{
#ifndef NDEBUG
  if (reversed_size < word_id * REVERSED_ENTRY_SIZE) {
    Log::error("Index::DiskIO::set_reversed_pointer: out of bounds access denied.");
  }
#endif
  *reinterpret_cast<ReversedBlock*>(&mmap_reversed[word_id * REVERSED_ENTRY_SIZE]) = reversed_block;
}

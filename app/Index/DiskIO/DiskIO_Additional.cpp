#include "../index.h"
#include "../../Helper/helper.h"
#include "../../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

AdditionalBlock* Index::DiskIO::get_additional_pointer(const ADDITIONAL_ID_TYPE id)
{
  return reinterpret_cast<AdditionalBlock*>(
    &mmap_additional[(id - 1) * ADDITIONAL_ENTRY_SIZE]);
}

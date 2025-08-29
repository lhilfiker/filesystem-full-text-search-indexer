#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <cstring>
#include <filesystem>
#include <string>

bool Index::last_updated_once() {
  if (!is_config_loaded)
    return false;

  if (std::filesystem::exists(CONFIG_INDEX_PATH / "lastupdated_mtime.info")) {
    return true;
  } else {
    return false;
  }
}
std::filesystem::file_time_type Index::last_updated_time() {
  if (!last_updated_once())
    Log::error("Invalid use of Index::last_updated_time. last_updated_once "
               "returns false. Should be checked first.");

  return std::filesystem::last_write_time(CONFIG_INDEX_PATH /
                                          "lastupdated_mtime.info");
}
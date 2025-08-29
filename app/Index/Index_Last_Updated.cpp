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
std::filesystem::file_time_type Index::last_updated_time(bool temp) {
  if (!last_updated_once() || is_config_loaded)
    Log::error("Invalid use of Index::last_updated_time. last_updated_once "
               "returns false. Should be checked first.");

  if (temp) {
    return std::filesystem::last_write_time(CONFIG_INDEX_PATH /
                                            "lastupdated_mtime_TEMP.info");
  } else {

    return std::filesystem::last_write_time(CONFIG_INDEX_PATH /
                                            "lastupdated_mtime.info");
  }
}

void Index::set_last_updated_time(std::filesystem::file_time_type new_time,
                                  bool temp) {
  if (!Index::lock(false)) {
    Log::write(
        3,
        "Index: set_last_updated_time failed because could not lock index. "
        "Will continue because this is non critcal, but it will mean the last "
        "updated staus could not be updated and you will need to index again.");
  }
  if (temp) {
    std::filesystem::last_write_time(
        CONFIG_INDEX_PATH / "lastupdated_mtime_TEMP.info", new_time);
  } else {
    std::filesystem::last_write_time(
        CONFIG_INDEX_PATH / "lastupdated_mtime.info", new_time);
  }
}

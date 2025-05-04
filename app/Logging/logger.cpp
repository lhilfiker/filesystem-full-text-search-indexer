#include "logging.h"
#include <array>
#include <iostream>
#include <string>

int log::min_log_level = 1;

void log::save_config(const int config_min_log_level) {
  // min_log_level
  if (config_min_log_level >= 1 && config_min_log_level <= 4) {
    min_log_level = config_min_log_level;
  } else {
    min_log_level = 3;
    write(3, "log: save_config: invalid min_log_level value. continuing with "
             "level 3 warn.");
  }
}

void log::write(const int log_level, const std::string &log_message) {
  // log_level: 1 = debug, 2 = info, 3 = warn, 4 = critical
  if (min_log_level <= log_level) {
    if (log_level >= 1 && log_level <= 4) {
      std::cout << log_level_text[log_level - 1] << log_message << "\n";
    } else {
      std::cout << "[INVALID LOG_LEVEL]: " << log_message << "\n";
    }
  }
}

void log::error(const std::string &error_message) {
  // for now, it will just log it and throw an error.
  write(4, "ERROR: " + error_message);
  throw 1;
}

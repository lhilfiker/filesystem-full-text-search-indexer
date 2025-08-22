#include "config.h"
#include "../Logging/logging.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::string> Config::internal_config{
    {"index_path", ""},
    {"lock_aquisition_timeout", "30"},
    {"config_scan_dot_paths", "false"},
    {"config_path_to_scan", ""},
    {"threads_to_use", "1"},
    {"config_local_index_memory", "50000"},
    {"config_min_log_level", "3"},
    {"config_exact_match", "false"},
    {"config_min_char_for_match", "4"}};

void Config::read_config(std::filesystem::path config_file_path) {}

void Config::update_value(std::pair<std::string, std::string> overwrite) {
  if (internal_config.count(overwrite.first) != 0) {
    internal_config[overwrite.first] = overwrite.second;
  }
}

bool Config::validate() { return true; }

void Config::set() {}

void Config::load(std::vector<std::pair<std::string, std::string>> overwrites,
                  std::filesystem::path config_file_path) {
  // overwrites is a key<->value list.

  read_config(config_file_path);

  // overwrite with cli options
  for (auto &element : overwrites) {
    update_value(element);
  }

  if (!validate()) {
    Log::error("Invalid config. Required options not set. Please set either in "
               "your config file or using commandline options. If you need "
               "help look at the documentation.");
  }

  // finally send the new values to the other classes.
  set();
}

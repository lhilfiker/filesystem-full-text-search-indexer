#include "config.h"
#include "../Index/index.h"
#include "../Indexer/indexer.h"
#include "../Logging/logging.h"
#include "../Search/search.h"
#include <fstream>
#include <sstream>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

std::map<std::string, std::string> Config::internal_config{
    {"index_path", ""},
    {"lock_aquisition_timeout", "30"},
    {"config_scan_dot_paths", "false"},
    {"config_path_to_scan", ""},
    {"config_threads_to_use", "1"},
    {"config_local_index_memory", "50000"},
    {"config_min_log_level", "3"},
    {"config_exact_match", "false"},
    {"config_min_char_for_match", "4"}};

void Config::update_value(std::pair<std::string, std::string> overwrite) {
  if (internal_config.count(overwrite.first) != 0) {
    internal_config[overwrite.first] = overwrite.second;
  }
}

void Config::read_config(std::filesystem::path config_file_path) {

  std::ifstream file(config_file_path);
  if (!file.is_open()) {
    // return when config does not exist because you can also set config via
    // CLI.
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream is_line(line);
    std::string key;
    if (std::getline(is_line, key, '=')) {
      std::string value;
      if (std::getline(is_line, value))
        update_value(std::make_pair(key, value));
    }
  }
}

bool Config::validate() {
  if (internal_config["index_path"] == "") {
    return false;
  }
  if (internal_config["config_path_to_scan"] == "") {
    return false;
  }
  return true;
}

void Config::set() {
  try {

    Index::save_config(internal_config["index_path"],
                       std::stoi(internal_config["lock_aquisition_timeout"]));
    indexer::save_config(
        internal_config["config_scan_dot_paths"] == "True" ? true : false,
        internal_config["config_path_to_scan"],
        std::stoi(internal_config["config_threads_to_use"]),
        std::stoi(internal_config["config_local_index_memory"]));
    Log::save_config(std::stoi(internal_config["config_min_log_level"]));
    Search::save_config(
        internal_config["config_exact_match"] == "true" ? true : false,
        std::stoi(internal_config["config_min_char_for_match"]));
  } catch (...) {
    Log::error("error when saving config in internal state. Please make sure "
               "all your values are correct.");
  }
}

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
  Log::write(1, "Config: Loaded config and is valid.");
}

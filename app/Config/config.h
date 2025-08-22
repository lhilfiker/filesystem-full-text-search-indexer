// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Config/Config.cpp
class Config {
private:
  static std::map<std::string, std::string> internal_config;
  void read_config(std::filesystem::path config_file_path);
  void update_value(std::pair<std::string, std::string>);
  bool validate();
  void set();

public:
  static void load(std::vector<std::pair<std::string, std::string>> overwrites,
                   std::filesystem::path config_file_path);
};

#endif
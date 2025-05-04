// logging.h
#ifndef LOGGING_H
#define LOGGING_H

#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Logging/logger.cpp
class log {
private:
  static int min_log_level;
  static constexpr std::array<std::string_view, 4> log_level_text = {
      "[DEBUG]: ", "[INFO]: ", "[WARN]: ", "[CRITICAL]: "};

public:
  static void save_config(const int config_min_log_level);
  static void write(const int log_level, const std::string &log_message);
  static void error(const std::string &error_message);
};

#endif
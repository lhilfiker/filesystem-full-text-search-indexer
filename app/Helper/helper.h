// helper.h
#ifndef HELPER_H
#define HELPER_H

#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Helper/helper.cpp
class helper {
private:
  static const std::array<char, 256> conversion_table;
  static const std::unordered_map<char, char> special_chars;

public:
  static int file_size(const std::filesystem::path &file_path);
  static std::string file_extension(const std::filesystem::path &path);
  static void convert_char(char &c);
};

#endif
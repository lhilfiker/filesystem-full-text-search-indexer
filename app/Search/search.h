// search.h
#ifndef SEARCH_H
#define SEARCH_H

#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Search/Search.cpp
class Search {
private:
  static bool config_exact_match;
  static uint8_t config_min_char_for_match;

public:
  static void save_config(bool exact_match, uint8_t min_char_for_match);
  static void search();
};

#endif
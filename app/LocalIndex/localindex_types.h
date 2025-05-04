// localindex_types.h
#ifndef LOCALINDEX_TYPES_H
#define LOCALINDEX_TYPES_H

#include "../Index/index_types.h"
#include "../index_config.h"
#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct words_reversed {
  std::string word;
  std::unordered_set<PATH_ID_TYPE> reversed;

  bool operator<(const words_reversed &other) const {
    return word < other.word;
  }
};

struct search_path_ids_return {
  PATH_ID_TYPE path_id;
  uint32_t count;

  bool operator<(const search_path_ids_return &other) const {
    return path_id < other.path_id;
  }
};

struct index_combine_data {
  std::vector<std::string> &paths;
  const size_t &paths_size;
  std::vector<uint32_t> &paths_count;
  const size_t &paths_count_size;
  std::vector<words_reversed> &words_and_reversed;
  const size_t &words_size;
  const size_t &reversed_size;
};

#endif
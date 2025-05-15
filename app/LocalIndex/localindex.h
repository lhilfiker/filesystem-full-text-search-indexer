// localindex.h
#ifndef LOCALINDEX_H
#define LOCALINDEX_H

#include "../Index/index_types.h"
#include "../lib/mio.hpp"
#include "localindex_types.h"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// LocalIndex/LocalIndex.cpp
class LocalIndex {
private:
  std::vector<std::string> paths;
  size_t paths_size;
  std::vector<words_reversed> words_and_reversed;
  size_t words_size;
  size_t reversed_size;
  std::vector<uint32_t> path_word_count;
  size_t path_word_count_size;
  bool sorted;

public:
  LocalIndex();
  size_t size();
  void clear();
  PATH_ID_TYPE add_path(const std::string &path_to_insert, const bool adding);
  void add_words(std::unordered_set<std::string> &words_to_insert,
                 PATH_ID_TYPE path_id);
  void sort();
  void add_to_disk();
  void combine(LocalIndex &to_combine_index);

  friend void combine(LocalIndex &to_combine_index);
};

#endif
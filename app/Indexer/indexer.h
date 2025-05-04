// indexer.h
#ifndef INDEXER_H
#define INDEXER_H

#include "../LocalIndex/localindex.h"
#include "../lib/mio.hpp"
#include "index_types.h"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Indexer/indexer.cpp
class indexer {
private:
  static bool config_loaded;
  static bool scan_dot_paths;
  static std::filesystem::path path_to_scan;
  static int threads_to_use;
  static size_t local_index_memory;

private:
  static bool extension_allowed(const std::filesystem::path &path);
  static std::unordered_set<std::string>
  get_words_text(const std::filesystem::path &path);
  static std::unordered_set<std::string>
  get_words(const std::filesystem::path &path);
  static LocalIndex
  thread_task(const std::vector<std::filesystem::path> paths_to_index);
  static bool queue_has_place(const std::vector<size_t> &batch_queue_added_size,
                              const size_t &filesize,
                              const size_t &max_filesize,
                              const uint32_t &current_batch_add_start);

public:
  static void save_config(const bool config_scan_dot_paths,
                          const std::filesystem::path &config_path_to_scan,
                          const int config_threads_to_use,
                          const size_t &config_local_index_memory);
  static int start_from();
};

#endif
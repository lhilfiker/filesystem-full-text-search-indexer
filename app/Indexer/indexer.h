// indexer.h
#ifndef INDEXER_H
#define INDEXER_H

#include "../LocalIndex/localindex.h"
#include "../lib/mio.hpp"
#include "indexer_types.h"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Indexer/Indexer.cpp
class indexer {
private:
  static bool config_loaded;
  static bool scan_dot_paths;
  static bool updated_files_only;
  static std::filesystem::path path_to_scan;
  static int threads_to_use;
  static int min_word_length;
  static int max_word_length;

  static size_t local_index_memory;

private:
  static bool extension_allowed(const std::filesystem::path &path);
  static std::unordered_set<std::string>
  get_words_utf8(const std::filesystem::path &path, const size_t start_loc,
                 const size_t end_loc);
  static std::unordered_set<std::string>
  get_words(const std::filesystem::path &path, const size_t start_loc,
            const size_t end_loc);
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
                          const size_t &config_local_index_memory,
                          const bool config_updated_files_only,
                          const int config_min_word_length,
                          const int config_max_word_length);
  static int start_from();
};

#endif
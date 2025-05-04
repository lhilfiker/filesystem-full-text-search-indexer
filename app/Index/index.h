// index.h
#ifndef INDEX_H
#define INDEX_H

#include "../LocalIndex/localindex.h"
#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Index/Index.cpp
class Index {
private:
  static bool is_config_loaded;
  static bool initialized;
  static bool is_mapped;
  static bool first_time;
  static int64_t paths_size;
  static int64_t paths_size_buffer;
  static int64_t paths_count_size;
  static int64_t paths_count_size_buffer;
  static int64_t words_size;
  static int64_t words_size_buffer;
  static int64_t words_f_size;
  static int64_t words_f_size_buffer;
  static int64_t reversed_size;
  static int64_t reversed_size_buffer;
  static int64_t additional_size;
  static int64_t additional_size_buffer;
  static mio::mmap_sink mmap_paths;
  static mio::mmap_sink mmap_paths_count;
  static mio::mmap_sink mmap_words;
  static mio::mmap_sink mmap_words_f;
  static mio::mmap_sink mmap_reversed;
  static mio::mmap_sink mmap_additional;

  // Config Values
  static std::filesystem::path CONFIG_INDEX_PATH;

private:
  static void check_files();
  static int map();
  static int sync_all();
  static int unmap();
  static void resize(const std::filesystem::path path_to_resize,
                     const size_t size);
  static int add_new(index_combine_data &index_to_add);
  static void add_reversed_to_word(index_combine_data &index_to_add,
                                   uint64_t &on_disk_count,
                                   std::vector<Transaction> &transactions,
                                   size_t &additional_new_needed_size,
                                   size_t &reversed_new_needed_size,
                                   uint64_t &on_disk_id,
                                   const size_t &local_word_count,
                                   PathsMapping &paths_mapping);
  static void add_new_word(index_combine_data &index_to_add,
                           uint64_t &on_disk_count,
                           std::vector<Transaction> &transactions,
                           std::vector<Insertion> &words_insertions,
                           std::vector<Insertion> &reversed_insertions,
                           size_t &additional_new_needed_size,
                           size_t &words_new_needed_size,
                           size_t &reversed_new_needed_size,
                           uint64_t &on_disk_id, const size_t &local_word_count,
                           PathsMapping &paths_mapping);
  static void insertion_to_transactions(
      std::vector<Transaction> &transactions,
      std::vector<Transaction> &move_transactions,
      std::vector<Insertion> &to_insertions,
      int index_type); // index_type: 1 = words, 3 = reversed.
  static void write_to_transaction(std::vector<Transaction> &transactions,
                                   mio::mmap_sink &mmap_transactions,
                                   size_t &transaction_file_location);
  static int merge(index_combine_data &index_to_add);
  static int execute_transactions();
  static std::vector<PATH_ID_TYPE> path_ids_from_word_id(uint64_t word_id);

public:
  static void save_config(const std::filesystem::path &index_path);
  static bool is_index_mapped();
  static int initialize();
  static int uninitialize();
  static int add(index_combine_data &index_to_add);
  static std::vector<search_path_ids_return>
  search_word_list(std::vector<std::string> &search_words, bool exact_match,
                   int min_char_for_match);
  static std::unordered_map<PATH_ID_TYPE, std::string>
  id_to_path_string(std::vector<search_path_ids_return> path_ids);
};

#endif
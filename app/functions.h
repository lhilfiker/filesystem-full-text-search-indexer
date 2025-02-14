// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "lib/english_stem.h"
#include "lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct words_reversed {
  std::wstring word;
  std::unordered_set<uint32_t> reversed;

  bool operator<(const words_reversed &other) const {
    return word < other.word;
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

// LocalIndex.cpp
class LocalIndex {
private:
  std::vector<std::string> paths;
  size_t paths_size;
  std::vector<words_reversed> words_and_reversed;
  size_t words_size;
  size_t reversed_size;
  std::vector<uint32_t> path_word_count;
  size_t path_word_count_size;

public:
  LocalIndex();
  int size();
  void clear();
  uint32_t add_path(const std::string &path_to_insert);
  void add_words(std::unordered_set<std::wstring> &words_to_insert,
                 uint32_t path_id);
  void sort();
  void add_to_disk();
  void combine(LocalIndex &to_combine_index);

  friend void combine(LocalIndex &to_combine_index);
};

struct threads_jobs {
  std::future<LocalIndex> future;
  uint32_t queue_id;
};

// indexer.cpp
class indexer {
private:
  static stemming::english_stem<> StemEnglish;
  static bool config_loaded;
  static bool scan_dot_paths;
  static std::filesystem::path path_to_scan;
  static int threads_to_use;
  static size_t local_index_memory;

private:
  static bool extension_allowed(const std::filesystem::path &path);
  static std::unordered_set<std::wstring>
  get_words_text(const std::filesystem::path &path);
  static std::unordered_set<std::wstring>
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

// helper.cpp
class helper {
private:
  static const std::array<char, 256> conversion_table;
  static const std::unordered_map<char, char> special_chars;

public:
  static int file_size(const std::filesystem::path &file_path);
  static std::string file_extension(const std::filesystem::path &path);
  static void convert_char(char &c);
};

// log.cpp
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

// Index.cpp

union TransactionHeader {
  struct {
    uint8_t status;          // 0 = no status, 1 = started, 2 = completed
    uint8_t index_type;      // paths = 0, words = 1, words_f = 2, reversed = 3,
                             // additional = 4, paths_count = 5
    uint64_t location;       // byte count in file, resize = 0.
    uint64_t backup_id;      // 0 = none. starting from 1 for  each transaction. each transaction has its own unique folder. same id for move and create a backup operation.
    uint8_t operation_type;  // 0 = Move, 1 = write, 2 = resize, 3 = create a backup
    uint64_t content_length; // length of content. for resize this indicates the
                             // new size. for move how much from the starting point and the same for create a backup.
  };
  unsigned char bytes[27];
};

struct Transaction {
  TransactionHeader header;
  std::string content; // For move operations this will be 8 bytes always and it is a uint64_t and signals the byte shift.
};

union InsertionHeader {
  struct {
    uint64_t location;
    uint64_t content_length;
  };
  unsigned char bytes[16];
};

struct Insertion {
  InsertionHeader header;
  std::string content;
};

struct PathsMapping {
  std::unordered_map<uint16_t, uint16_t> by_local;
  std::unordered_map<uint16_t, uint16_t> by_disk;
};

class Index {
private:
  static bool is_config_loaded;
  static bool is_mapped;
  static bool first_time;
  static int buffer_size;
  static std::filesystem::path index_path;
  static int paths_size;
  static int paths_size_buffer;
  static int paths_count_size;
  static int paths_count_size_buffer;
  static int words_size;
  static int words_size_buffer;
  static int words_f_size;
  static int words_f_size_buffer;
  static int reversed_size;
  static int reversed_size_buffer;
  static int additional_size;
  static int additional_size_buffer;
  static mio::mmap_sink mmap_paths;
  static mio::mmap_sink mmap_paths_count;
  static mio::mmap_sink mmap_words;
  static mio::mmap_sink mmap_words_f;
  static mio::mmap_sink mmap_reversed;
  static mio::mmap_sink mmap_additional;

private:
  static void check_files();
  static int get_actual_size(const mio::mmap_sink &mmap);
  static int map();
  static int unmap();
  static void resize(const std::filesystem::path &path_to_resize,
                     const int size);
  static int add_new(index_combine_data &index_to_add);
  static void add_reversed_to_word(index_combine_data &index_to_add,
                                   uint64_t &on_disk_count,
                                   std::vector<Transaction> &transactions,
                                   size_t &additional_new_needed_size,
                                   uint32_t &on_disk_id,
                                   const size_t &local_word_count,
                                   PathsMapping &paths_mapping);
  static void add_new_word(index_combine_data &index_to_add,
                           uint64_t &on_disk_count,
                           std::vector<Transaction> &transactions,
                           std::vector<Insertion> words_insertions,
                           std::vector<Insertion> reversed_insertions,
                           size_t &additional_new_needed_size,
                           size_t &words_new_needed_size,
                           size_t &reversed_new_needed_size,
                           uint32_t &on_disk_id, const size_t &local_word_count,
                           PathsMapping &paths_mapping);
  static int merge(index_combine_data &index_to_add);

public:
  static void save_config(const std::filesystem::path &config_index_path,
                          const int config_buffer_size);
  static bool is_index_mapped();
  static int initialize();
  static int uninitialize();
  static int add(index_combine_data &index_to_add);
};

#endif

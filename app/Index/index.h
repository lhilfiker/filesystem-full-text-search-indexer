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
class Index
{
private:
  class DiskIO
  {
  private:
    mio::mmap_sink mmap_paths;
    mio::mmap_sink mmap_paths_count;
    mio::mmap_sink mmap_words;
    mio::mmap_sink mmap_words_f;
    mio::mmap_sink mmap_reversed;
    mio::mmap_sink mmap_additional;
    size_t paths_size;
    size_t paths_count_size;
    size_t words_size;
    size_t words_f_size;
    size_t reversed_size;
    size_t additional_size;
    bool additional_mapped;

    bool is_mapped;
    bool is_locked;

    bool map_file(mio::mmap_sink& target_mmap, size_t& target_size, const std::string& source_path);
    bool sync_file(mio::mmap_sink& target_mmap);

  public:
    DiskIO();

    bool map(const std::filesystem::path& index_path);
    // maps the index files.
    // returns true if successful , false if not
    bool unmap();
    // unmaps the index files.
    // returns true if successful , false if not
    bool sync_all();


    // getters
    size_t get_paths_size() const;
    size_t get_paths_count_size() const;
    size_t get_words_size() const;
    size_t get_words_f_size() const;
    size_t get_reversed_size() const;
    size_t get_additional_size() const;
    bool get_mapped() const;
    bool get_locked() const;
    AdditionalBlock* get_additional_pointer(const ADDITIONAL_ID_TYPE id);
    ReversedBlock* get_reversed_pointer(const size_t id);
    WORD_SEPARATOR_TYPE get_word_separator(const size_t start_pos) const;
    char get_char_of_word(size_t word_start_pos, int char_pos) const;
    std::vector<WordsFValue> get_words_f() const;
    uint16_t get_path_separator(size_t start_pos) const;
    std::string get_path(size_t start_pos, uint16_t length) const;
    uint32_t get_path_count(size_t path_id) const;
  };

private:
  static DiskIO disk_io;

  static bool is_config_loaded;
  static bool initialized;
  static bool readonly_initialized;
  static bool is_mapped;
  static bool first_time;
  static int64_t paths_size_buffer;
  static int64_t paths_count_size_buffer;
  static int64_t words_size_buffer;
  static int64_t words_f_size_buffer;
  static int64_t reversed_size_buffer;
  static int64_t additional_size_buffer;


  static bool index_lock;
  static bool read_only;

  // Config Values
  static std::filesystem::path CONFIG_INDEX_PATH;
  static uint16_t CONFIG_LOCK_ACQUISITION_TIMEOUT;

private:
  static void check_files();
  static int sync_all();
  static void resize(const std::filesystem::path path_to_resize,
                     const size_t size);
  static int add_new(index_combine_data& index_to_add);
  static std::string generate_new_additionals(index_combine_data& index_to_add, const size_t& local_word_count,
                                              PathsMapping& paths_mapping, uint32_t& current_additional);
  static void add_reversed_to_word(index_combine_data& index_to_add,
                                   std::vector<Transaction>& transactions,
                                   size_t& additional_new_needed_size,
                                   size_t& reversed_new_needed_size,
                                   uint64_t& on_disk_id,
                                   const size_t& local_word_count,
                                   PathsMapping& paths_mapping);
  static void add_new_word(index_combine_data& index_to_add,
                           uint64_t& on_disk_count,
                           std::vector<Transaction>& transactions,
                           std::vector<Insertion>& words_insertions,
                           std::vector<Insertion>& reversed_insertions,
                           size_t& additional_new_needed_size,
                           size_t& words_new_needed_size,
                           size_t& reversed_new_needed_size,
                           uint64_t& on_disk_id, const size_t& local_word_count,
                           PathsMapping& paths_mapping);
  static void insertion_to_transactions(
    std::vector<Transaction>& transactions,
    std::vector<Transaction>& move_transactions,
    std::vector<Insertion>& to_insertions,
    int index_type); // index_type: 1 = words, 3 = reversed.
  static void write_to_transaction(std::vector<Transaction>& transactions,
                                   mio::mmap_sink& mmap_transactions,
                                   size_t& transaction_file_location);
  static int merge(index_combine_data& index_to_add);
  static int execute_transactions();
  static std::vector<PATH_ID_TYPE> path_ids_from_word_id(uint64_t word_id);
  static void lock_update_sizes();
  static int
  write_transaction_file(const std::filesystem::path& transaction_path,
                         std::vector<Transaction>& move_transactions,
                         std::vector<Transaction>& transactions);

public:
  static void save_config(const std::filesystem::path& index_path,
                          const uint16_t lock_aquisition_timeout);
  static bool is_index_mapped();
  static int initialize();
  static int uninitialize();
  static int add(index_combine_data& index_to_add);
  static std::vector<search_path_ids_count_return>
  search_word_list(std::vector<std::pair<std::string, bool>>& search_words,
                   int min_char_for_match);
  static std::unordered_map<PATH_ID_TYPE, std::string>
  id_to_path_string(std::vector<search_path_ids_return> path_ids);

  static int lock_status(bool initialize);
  static bool lock(bool initialize);
  static bool unlock(bool initialize);
  static bool health_status();
  static bool last_updated_once(bool temp);
  static std::filesystem::file_time_type last_updated_time(bool temp);
  static void set_last_updated_time(std::filesystem::file_time_type new_time);
  static void mark_current_time_temp();
  static bool expensive_index_check(const bool verbose_output);
};

#endif

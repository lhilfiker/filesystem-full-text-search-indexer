#include "functions.h"
#include "lib/mio.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

bool Index::is_config_loaded = false;
bool Index::is_mapped = false;
bool Index::first_time = false;
int Index::buffer_size = 0;
std::filesystem::path Index::index_path;
int Index::paths_size = 0;
int Index::paths_size_buffer = 0;
int Index::paths_count_size = 0;
int Index::paths_count_size_buffer = 0;
int Index::words_size = 0;
int Index::words_size_buffer = 0;
int Index::words_f_size = 0;
int Index::words_f_size_buffer = 0;
int Index::reversed_size = 0;
int Index::reversed_size_buffer = 0;
int Index::additional_size = 0;
int Index::additional_size_buffer = 0;
mio::mmap_sink Index::mmap_paths;
mio::mmap_sink Index::mmap_paths_count;
mio::mmap_sink Index::mmap_words;
mio::mmap_sink Index::mmap_words_f;
mio::mmap_sink Index::mmap_reversed;
mio::mmap_sink Index::mmap_additional;

void Index::save_config(const std::filesystem::path &config_index_path,
                        const int config_buffer_size) {
  index_path = config_index_path;
  buffer_size = config_buffer_size;
  if (buffer_size < 10000) {
    buffer_size = 10000;
    log::write(3, "Index: save_config: buffer size needs to be atleast 10000. "
                  "setting it to 10000.");
  }
  if (buffer_size > 10000000) {
    buffer_size = 10000000;
    log::write(3, "Index: save_config: buffer size can not be larger than "
                  "~10MB, setting it to ~10MB");
  }
  is_config_loaded = true;
  log::write(1, "Index: save_config: saved config successfully.");
  return;
}

bool Index::is_index_mapped() { return is_mapped; }

void Index::resize(const std::filesystem::path path_to_resize,
                   const size_t size) {
  std::error_code ec;
  std::filesystem::resize_file(path_to_resize, size);
  if (ec) {
    log::error("Index: resize: could not resize file at path: " +
               (path_to_resize).string() +
               " with size: " + std::to_string(size) + ".");
  }
  log::write(1, "Index: resize: resized file successfully.");
  return;
}

int Index::unmap() {
  std::error_code ec;
  if (mmap_paths.is_mapped()) {
    mmap_paths.unmap();
  }
  if (mmap_paths_count.is_mapped()) {
    mmap_paths_count.unmap();
  }
  if (mmap_words.is_mapped()) {
    mmap_words.unmap();
  }
  if (mmap_words_f.is_mapped()) {
    mmap_words_f.unmap();
  }
  if (mmap_reversed.is_mapped()) {
    mmap_reversed.unmap();
  }
  if (mmap_additional.is_mapped()) {
    mmap_additional.unmap();
  }
  if (ec) {
    log::write(4, "Index: unmap: error when unmapping index files.");
    return 1;
  }
  log::write(1, "Index: unmap: unmapped all successfully.");
  return 0;
}

int Index::sync_all() {
  std::error_code ec;
  mmap_paths.sync(ec);
  mmap_words.sync(ec);
  mmap_words_f.sync(ec);
  mmap_reversed.sync(ec);
  mmap_additional.sync(ec);
  mmap_paths_count.sync(ec);
  if (ec)
    return 1;
  return 0;
}

int Index::map() {
  std::error_code ec;
  if (!mmap_paths.is_mapped()) {
    mmap_paths = mio::make_mmap_sink((index_path / "paths.index").string(), 0,
                                     mio::map_entire_file, ec);
  }
  if (!mmap_paths_count.is_mapped()) {
    mmap_paths_count =
        mio::make_mmap_sink((index_path / "paths_count.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_words.is_mapped()) {
    mmap_words = mio::make_mmap_sink((index_path / "words.index").string(), 0,
                                     mio::map_entire_file, ec);
  }
  if (!mmap_words_f.is_mapped()) {
    mmap_words_f = mio::make_mmap_sink((index_path / "words_f.index").string(),
                                       0, mio::map_entire_file, ec);
  }
  if (!mmap_reversed.is_mapped()) {
    mmap_reversed = mio::make_mmap_sink(
        (index_path / "reversed.index").string(), 0, mio::map_entire_file, ec);
  }
  if (!mmap_additional.is_mapped()) {
    mmap_additional =
        mio::make_mmap_sink((index_path / "additional.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (ec) {
    log::write(3, "Index: map: error when mapping index files.");
    return 1;
  }
  log::write(1, "Index: map: mapped all successfully.");
  return 0;
}

void Index::check_files() {
  if (!is_config_loaded) {
    log::error("Index: check_files: config not loaded. can not continue.");
  }
  log::write(1, "Index: check_files: checking files.");
  std::error_code ec;
  if (!std::filesystem::is_directory(index_path)) {
    log::write(1, "Index: check_files: creating index directory.");
    std::filesystem::create_directories(index_path);
  }
  if (!std::filesystem::is_directory(index_path / "transaction")) {
    log::write(1, "Index: check_files: creating transaction directory.");
    std::filesystem::create_directories(index_path / "transaction");
  }
  if (!std::filesystem::is_directory(index_path / "transaction" / "backups")) {
    log::write(1,
               "Index: check_files: creating transaction backups directory.");
    std::filesystem::create_directories(index_path / "transaction" / "backups");
  }

  if (std::filesystem::exists(index_path / "firsttimewrite.info")) {
    // If this is still here it means that the first time write failed. delete
    // all index files.
    log::write(2, "Index: check_files: First time write didn't complete last "
                  "time. Index possibly corrupt. Deleting to start fresh.");
    std::filesystem::remove(index_path / "paths.index");
    std::filesystem::remove(index_path / "paths_count.index");
    std::filesystem::remove(index_path / "words.index");
    std::filesystem::remove(index_path / "words_f.index");
    std::filesystem::remove(index_path / "reversed.index");
    std::filesystem::remove(index_path / "additional.index");
    std::filesystem::remove(index_path / "firsttimewrite.info");
    std::filesystem::remove(index_path / "transaction" / "transaction.list");
  }

  if (!std::filesystem::exists(index_path / "paths.index") ||
      helper::file_size(index_path / "paths.index") == 0 ||
      !std::filesystem::exists(index_path / "paths_count.index") ||
      helper::file_size(index_path / "paths_count.index") == 0 ||
      !std::filesystem::exists(index_path / "words.index") ||
      helper::file_size(index_path / "words.index") == 0 ||
      !std::filesystem::exists(index_path / "words_f.index") ||
      helper::file_size(index_path / "words_f.index") == 0 ||
      !std::filesystem::exists(index_path / "reversed.index") ||
      helper::file_size(index_path / "reversed.index") == 0 ||
      !std::filesystem::exists(index_path / "additional.index")
      // Additional size can be 0 if there aren't any words with more than 4
      // linked paths.
  ) {
    first_time = true;
    log::write(
        1,
        "Index: check_files: index files damaged / not existing, recreating.");
    std::ofstream{index_path / "paths.index"};
    std::ofstream{index_path / "paths_count.index"};
    std::ofstream{index_path / "words.index"};
    std::ofstream{index_path / "words_f.index"};
    std::ofstream{index_path / "reversed.index"};
    std::ofstream{index_path / "additional.index"};
    // Remove Transacition file if exist
    std::filesystem::remove(index_path / "transaction" / "transaction.list");
  }
  if (ec) {
    log::error("Index: check_files: error accessing/creating index files in " +
               (index_path).string() + ".");
  }
  return;
}

int Index::initialize() {
  if (!is_config_loaded) {
    log::write(4, "Index: initialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  std::error_code ec;
  unmap();       // unmap anyway incase they are already mapped.
  check_files(); // check if index files exist and create them.
  map();         // ignore error here as it might fail if file size is 0.
  // get actual sizes of the files.
  paths_size = helper::file_size(index_path / "paths.index");
  paths_count_size = helper::file_size(index_path / "paths_count.index");
  words_size = helper::file_size(index_path / "words.index");
  words_f_size = helper::file_size(index_path / "words_f.index");
  reversed_size = helper::file_size(index_path / "reversed.index");
  additional_size = helper::file_size(index_path / "additional.index");
  // If an error occured exit.
  if (paths_size == -1 || words_size == -1 || words_f_size == -1 ||
      reversed_size == -1 || additional_size == -1) {
    log::error("Index: initialize: could not get actual size of index "
               "files, exiting.");
    return 1;
  }
  is_mapped = true;

  // check if transaction file exists
  if (std::filesystem::exists(index_path / "transaction" /
                              "transaction.list")) {
    log::write(2, "Index: a transaction file still exists. Checking if Index "
                  "needs to be repaired.");
    if (execute_transactions() == 1) {
      log::error(
          "Index: while checking transaction file an error occured. Exiting.");
    }
    log::write(
        2, "Index: transaction file successfully checked. Finishing startup.");
  }
  // removing all backups because they are not needed anymore.
  std::filesystem::remove_all(index_path / "transaction" / "backups");
  if (!std::filesystem::is_directory(index_path / "transaction" / "backups")) {
    log::write(1,
               "Index: intitialize: creating transactions backups directory.");
    std::filesystem::create_directories(index_path / "transaction" / "backups");
  }

  return 0;
}

int Index::uninitialize() {
  if (!is_config_loaded) {
    log::write(4, "Index: uninitialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  // write caches, etc...
  if (unmap() == 1) {
    log::write(4, "Index: uninitialize: could not unmap.");
    return 1;
  }
  return 0;
}

int Index::add(index_combine_data &index_to_add) {
  std::error_code ec;
  if (first_time) {
    first_time = false;
    std::ofstream{index_path /
                  "firsttimewrite.info"}; // add file to signal firsttimewrite
                                          // is in progress. Delete after it is
                                          // successfully done.
    if (add_new(index_to_add) == 0) {
      std::filesystem::remove(index_path / "firsttimewrite.info");
      return 0;
    } else {
      // if an error occured exit.
      log::error("Index: Frist time write failed. Exiting. Corupted index "
                 "files will be deleted on startup.");
    }
  } else {
    if (merge(index_to_add) == 1)
      return 1;
    return execute_transactions();
  }
  return 0;
}

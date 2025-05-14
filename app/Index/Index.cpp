#include "index.h"
#include "../Helper/helper.h"
#include "../Logging/logging.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

bool Index::is_config_loaded = false;
bool Index::initialized = false;
bool Index::readonly_initialized = true;
bool Index::is_mapped = false;
bool Index::first_time = false;
int64_t Index::paths_size = 0;
int64_t Index::paths_size_buffer = 0;
int64_t Index::paths_count_size = 0;
int64_t Index::paths_count_size_buffer = 0;
int64_t Index::words_size = 0;
int64_t Index::words_size_buffer = 0;
int64_t Index::words_f_size = 0;
int64_t Index::words_f_size_buffer = 0;
int64_t Index::reversed_size = 0;
int64_t Index::reversed_size_buffer = 0;
int64_t Index::additional_size = 0;
int64_t Index::additional_size_buffer = 0;
mio::mmap_sink Index::mmap_paths;
mio::mmap_sink Index::mmap_paths_count;
mio::mmap_sink Index::mmap_words;
mio::mmap_sink Index::mmap_words_f;
mio::mmap_sink Index::mmap_reversed;
mio::mmap_sink Index::mmap_additional;

// Config Values, they will be set once and never again while the index is
// initialized
std::filesystem::path Index::CONFIG_INDEX_PATH;
uint16_t Index::CONFIG_LOCK_ACQUISITION_TIMEOUT = 30;

void Index::save_config(const std::filesystem::path &index_path,
                        const uint16_t lock_aquisition_timeout) {
  if (initialized) {
    Log::write(3, "Index: save_config: Index was already initialzed, can not "
                  "save config. Try to uninitialize first.");
  }
  CONFIG_INDEX_PATH =
      index_path; // Index path validation happens in check_files

  CONFIG_LOCK_ACQUISITION_TIMEOUT = 30; // set to default
  if (lock_aquisition_timeout >= 0 && lock_aquisition_timeout <= 30000) {
    CONFIG_LOCK_ACQUISITION_TIMEOUT = lock_aquisition_timeout;
  }
  is_config_loaded = true;
  Log::write(1, "Index: save_config: saved config successfully.");
  return;
}

bool Index::is_index_mapped() { return is_mapped; }

void Index::resize(const std::filesystem::path path_to_resize,
                   const size_t size) {
  std::error_code ec;
  std::filesystem::resize_file(path_to_resize, size);
  if (ec) {
    Log::error("Index: resize: could not resize file at path: " +
               (path_to_resize).string() +
               " with size: " + std::to_string(size) + ".");
  }
  Log::write(1, "Index: resize: resized file successfully.");
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
    Log::write(4, "Index: unmap: error when unmapping index files.");
    return 1;
  }
  Log::write(1, "Index: unmap: unmapped all successfully.");
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
    mmap_paths =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "paths.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_paths_count.is_mapped()) {
    mmap_paths_count =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "paths_count.index").string(),
                            0, mio::map_entire_file, ec);
  }
  if (!mmap_words.is_mapped()) {
    mmap_words =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "words.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_words_f.is_mapped()) {
    mmap_words_f =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "words_f.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_reversed.is_mapped()) {
    mmap_reversed =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "reversed.index").string(), 0,
                            mio::map_entire_file, ec);
  }
  if (!mmap_additional.is_mapped()) {
    mmap_additional =
        mio::make_mmap_sink((CONFIG_INDEX_PATH / "additional.index").string(),
                            0, mio::map_entire_file, ec);
  }
  if (ec) {
    Log::write(3, "Index: map: error when mapping index files.");
    return 1;
  }
  Log::write(1, "Index: map: mapped all successfully.");
  return 0;
}

void Index::check_files() {
  if (!is_config_loaded) {
    Log::error("Index: check_files: config not loaded. can not continue.");
  }
  Log::write(1, "Index: check_files: checking files.");
  std::error_code ec;
  if (!std::filesystem::is_directory(CONFIG_INDEX_PATH)) {
    if (readonly_initialized) {
      Log::error(
          "Index: check_files: Can not continue, Index is readonly initialized "
          "but needs writeable action to even be readnly. Exiting");
    }
    Log::write(1, "Index: check_files: creating index directory.");
    std::filesystem::create_directories(CONFIG_INDEX_PATH);
  }
  if (!std::filesystem::is_directory(CONFIG_INDEX_PATH / "transaction")) {
    if (readonly_initialized) {
      Log::write(1, "Index: check_files: creating transaction directory.");
      std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction");
    }
  }
  if (!std::filesystem::is_directory(CONFIG_INDEX_PATH / "transaction" /
                                     "backups")) {
    if (readonly_initialized) {
      Log::write(1,
                 "Index: check_files: creating transaction backups directory.");
      std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction" /
                                          "backups");
    }
  }

  if (std::filesystem::exists(CONFIG_INDEX_PATH / "firsttimewrite.info")) {
    // If this is still here it means that the first time write failed. delete
    // all index files.
    if (readonly_initialized) {
      Log::error(
          "Index: check_files: Can not continue, Index is readonly initialized "
          "but needs writeable action to even be readnly. Exiting");
    }
    Log::write(2, "Index: check_files: First time write didn't complete last "
                  "time. Index possibly corrupt. Deleting to start fresh.");
    std::filesystem::remove(CONFIG_INDEX_PATH / "paths.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "paths_count.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "words.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "words_f.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "reversed.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "additional.index");
    std::filesystem::remove(CONFIG_INDEX_PATH / "firsttimewrite.info");
    std::filesystem::remove(CONFIG_INDEX_PATH / "transaction" /
                            "transaction.list");
  }

  if (!std::filesystem::exists(CONFIG_INDEX_PATH / "paths.index") ||
      Helper::file_size(CONFIG_INDEX_PATH / "paths.index") == 0 ||
      !std::filesystem::exists(CONFIG_INDEX_PATH / "paths_count.index") ||
      Helper::file_size(CONFIG_INDEX_PATH / "paths_count.index") == 0 ||
      !std::filesystem::exists(CONFIG_INDEX_PATH / "words.index") ||
      Helper::file_size(CONFIG_INDEX_PATH / "words.index") == 0 ||
      !std::filesystem::exists(CONFIG_INDEX_PATH / "words_f.index") ||
      Helper::file_size(CONFIG_INDEX_PATH / "words_f.index") == 0 ||
      !std::filesystem::exists(CONFIG_INDEX_PATH / "reversed.index") ||
      Helper::file_size(CONFIG_INDEX_PATH / "reversed.index") == 0 ||
      !std::filesystem::exists(CONFIG_INDEX_PATH / "additional.index")
      // Additional size can be 0 if there aren't any words with more than 4
      // linked paths.
  ) {
    if (readonly_initialized) {
      Log::error(
          "Index: check_files: Can not continue, Index is readonly initialized "
          "but needs writeable action to even be readnly. Exiting");
    }
    first_time = true;
    Log::write(
        1,
        "Index: check_files: index files damaged / not existing, recreating.");
    std::ofstream{CONFIG_INDEX_PATH / "paths.index"};
    std::ofstream{CONFIG_INDEX_PATH / "paths_count.index"};
    std::ofstream{CONFIG_INDEX_PATH / "words.index"};
    std::ofstream{CONFIG_INDEX_PATH / "words_f.index"};
    std::ofstream{CONFIG_INDEX_PATH / "reversed.index"};
    std::ofstream{CONFIG_INDEX_PATH / "additional.index"};
    // Remove Transacition file if exist
    std::filesystem::remove(CONFIG_INDEX_PATH / "transaction" /
                            "transaction.list");
  }
  if (ec) {
    Log::error("Index: check_files: error accessing/creating index files in " +
               (CONFIG_INDEX_PATH).string() + ".");
  }
  return;
}

int Index::initialize() {
  if (!is_config_loaded) {
    Log::write(4, "Index: initialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  std::error_code ec;
  readonly_initialized = true;
  unlock(true); // to remove potential leftover lock.
  if (lock_status(true) == 1) {
    readonly_initialized = false;
  }
  // just to update internal state
  unmap();       // unmap anyway incase they are already mapped.
  check_files(); // check if index files exist and create them.
  map();         // ignore error here as it might fail if file size is 0.
  // get actual sizes of the files.
  paths_size = Helper::file_size(CONFIG_INDEX_PATH / "paths.index");
  paths_count_size = Helper::file_size(CONFIG_INDEX_PATH / "paths_count.index");
  words_size = Helper::file_size(CONFIG_INDEX_PATH / "words.index");
  words_f_size = Helper::file_size(CONFIG_INDEX_PATH / "words_f.index");
  reversed_size = Helper::file_size(CONFIG_INDEX_PATH / "reversed.index");
  additional_size = Helper::file_size(CONFIG_INDEX_PATH / "additional.index");
  // If an error occured exit.
  if (paths_size == -1 || words_size == -1 || words_f_size == -1 ||
      reversed_size == -1 || additional_size == -1) {
    Log::error("Index: initialize: could not get actual size of index "
               "files, exiting.");
    return 1;
  }
  is_mapped = true;

  // check if transaction file exists
  if (readonly_initialized) {
    if (std::filesystem::exists(CONFIG_INDEX_PATH / "transaction" /
                                "transaction.list")) {
      Log::write(2, "Index: a transaction file still exists. Checking if Index "
                    "needs to be repaired.");
      if (execute_transactions() == 1) {
        Log::error("Index: while checking transaction file an error occured. "
                   "Exiting.");
      }
      Log::write(
          2,
          "Index: transaction file successfully checked. Finishing startup.");
    }
    // removing all backups because they are not needed anymore.
    std::filesystem::remove_all(CONFIG_INDEX_PATH / "transaction" / "backups");
    if (!std::filesystem::is_directory(CONFIG_INDEX_PATH / "transaction" /
                                       "backups")) {
      Log::write(
          1, "Index: intitialize: creating transactions backups directory.");
      std::filesystem::create_directories(CONFIG_INDEX_PATH / "transaction" /
                                          "backups");
    }
  }

  initialized = true;

  return 0;
}

int Index::uninitialize() {
  if (!is_config_loaded) {
    Log::write(4, "Index: uninitialize: config not loaded. can not continue.");
    return 1;
  }
  is_mapped = false;
  // write caches, etc...
  if (unmap() == 1) {
    Log::write(4, "Index: uninitialize: could not unmap.");
    return 1;
  }
  unlock(true);
  initialized = false;
  readonly_initialized = true;
  return 0;
}

int Index::add(index_combine_data &index_to_add) {
  std::error_code ec;
  if (!initialized || lock_status(false) <= 0) {
    Log::write(3, "Index: Can not add because index is not initialized or "
                  "Index is locked and read-only most likely due to another "
                  "process doing a merge or first time add.");
    return 1;
  }
  if (readonly_initialized) {
    // if it is readonly initialized we will try to reinitialize and continue;
    initialize();
    if (readonly_initialized || !initialized) {
      // if it is still readonly we return
      Log::write(
          3, "Index: could not initialize wihtout becomign readonly, this "
             "means some initialization were not completed and manipulating "
             "the index now could result in Data loss, this is most likely "
             "because another process is writing to it right now.");
      return 1;
    }
  }
  if (!lock(false)) {
    // Acquire lock
    Log::write(3, "Index: Can not add because acquiring lock failed. Check "
                  "previous logs");
    return 1; // failed to acquire lock.
  }
  if (first_time) {
    first_time = false;
    std::ofstream{CONFIG_INDEX_PATH /
                  "firsttimewrite.info"}; // add file to signal firsttimewrite
                                          // is in progress. Delete after it
                                          // is successfully done.
    if (add_new(index_to_add) == 0) {
      std::filesystem::remove(CONFIG_INDEX_PATH / "firsttimewrite.info");
      unlock(false); // unlock after done
      return 0;
    } else {
      // if an error occured exit.
      Log::error("Index: Frist time write failed. Exiting. Corupted index "
                 "files will be deleted on startup.");
    }
  } else {
    if (merge(index_to_add) == 1) {
      unlock(false); // unlock after done
      return 1;
    }
    execute_transactions();
  }
  unlock(false); // unlock after done
  return 0;
}

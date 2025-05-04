#include "indexer.h"
#include "../Helper/helper.h"
#include "../Logging/logging.h"
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

bool indexer::config_loaded = false;
bool indexer::scan_dot_paths = false;
std::filesystem::path indexer::path_to_scan;
int indexer::threads_to_use = 1;
size_t indexer::local_index_memory = 5000000;

void indexer::save_config(const bool config_scan_dot_paths,
                          const std::filesystem::path &config_path_to_scan,
                          const int config_threads_to_use,
                          const size_t &config_local_index_memory) {
  std::error_code ec;
  if (config_threads_to_use < 1) {
    threads_to_use = 1;
  } else if (config_threads_to_use > std::thread::hardware_concurrency()) {
    threads_to_use = std::thread::hardware_concurrency();
  } else {
    threads_to_use = config_threads_to_use;
  }
  if (ec) {
    threads_to_use = 1;
  }
  if (config_local_index_memory >
      5000000) { // ignore larger memory as most modern system will handle that
    local_index_memory = config_local_index_memory;
  }
  scan_dot_paths = config_scan_dot_paths;
  path_to_scan = config_path_to_scan;
  log::write(1, "indexer: save_config: saved config successfully.");
  config_loaded = true;
  return;
}

bool indexer::extension_allowed(const std::filesystem::path &path) {
  if (path.extension() == ".txt" || path.extension() == ".md")
    return true;
  return false;
}

std::unordered_set<std::string>
indexer::get_words_text(const std::filesystem::path &path) {
  // This will need to be redone.
  // It needs to consider Character encoding and proper error handling.

  std::error_code ec;
  mio::mmap_source file;
  std::unordered_set<std::string> words_return{};
  file.map(path.string(), ec);
  std::string current_word = "";
  if (!ec) {
    for (char c : file) {
      helper::convert_char(c);
      if (c == '!') {
        if (current_word.length() > 4 && current_word.length() < 15) {
          // stem word
          words_return.insert(current_word);
        }
        current_word.clear();
      } else {
        current_word.push_back(c);
      }
    }
  }
  if (current_word.length() > 3 && current_word.length() < 20) {
    // stem word
    words_return.insert(current_word);
  }
  file.unmap();
  // file name
  current_word.clear();
  for (char c : path.filename().string()) {
    helper::convert_char(c);
    if (c == '!') {
      if (current_word.length() > 4 && current_word.length() < 15) {
        // stem word
        words_return.insert(current_word);
      }
      current_word.clear();
    } else {
      current_word.push_back(c);
    }
  }
  if (current_word.length() > 3 && current_word.length() < 20) {
    // stem word
    words_return.insert(current_word);
  }

  if (ec) {
    log::write(3, "indexer: get_words: error reading / normalizing file.");
  }
  return words_return;
}

std::unordered_set<std::string>
indexer::get_words(const std::filesystem::path &path) {
  std::string extension = helper::file_extension(path);

  if (extension == ".txt")
    return get_words_text(path);

  return std::unordered_set<std::string>{};
}

LocalIndex
indexer::thread_task(const std::vector<std::filesystem::path> paths_to_index) {
  std::error_code ec;
  log::write(1, "indexer: thread_task: new task started.");
  LocalIndex task_index;
  for (const std::filesystem::path &path : paths_to_index) {
    log::write(1, "indexer: indexing path: " + path.string());
    PATH_ID_TYPE path_id = task_index.add_path(path);
    std::unordered_set<std::string> words_to_add = get_words(path);
    task_index.add_words(words_to_add, path_id);
    if (ec) {
    }
  }
  return task_index;
}

bool indexer::queue_has_place(const std::vector<size_t> &batch_queue_added_size,
                              const size_t &filesize,
                              const size_t &max_filesize,
                              const uint32_t &current_batch_add_start) {
  if (batch_queue_added_size.size() <= current_batch_add_start)
    return true;
  for (int i = current_batch_add_start;
       i < current_batch_add_start + threads_to_use; ++i) {
    if (batch_queue_added_size[i] + filesize <= max_filesize) {
      return true;
    }
  }
  return false;
}

int indexer::start_from() {
  if (!config_loaded) {
    return 1;
  }
  std::error_code ec;
  LocalIndex index;
  log::write(2, "indexer: starting scan from last successful scan.");
  std::vector<std::vector<std::filesystem::path>> queue(threads_to_use);
  std::vector<std::filesystem::path> too_big_files;
  size_t current_thread_filesize = 0;
  size_t thread_max_filesize = local_index_memory / threads_to_use;

  uint16_t thread_counter = 0;

  uint32_t current_batch_add_running = 0;
  uint32_t current_batch_add_next = 0;
  uint32_t batch_queue_add_start = 0;
  std::vector<size_t> batch_queue_added_size(threads_to_use);
  uint32_t batch_queue_current = 0;
  size_t paths_count = 0;
  size_t paths_size = 0;
  bool needs_a_queue = true;
  std::vector<threads_jobs> async_awaits;
  async_awaits.reserve(threads_to_use);

  for (const auto &dir_entry : std::filesystem::recursive_directory_iterator(
           path_to_scan,
           std::filesystem::directory_options::skip_permission_denied, ec)) {
    if (extension_allowed(dir_entry.path()) &&
        !std::filesystem::is_directory(dir_entry, ec) &&
        dir_entry.path().string().find("/.") == std::string::npos) {
      size_t filesize = std::filesystem::file_size(dir_entry.path(), ec);
      if (ec)
        continue;
      if (filesize > thread_max_filesize) {
        log::write(1,
                   "indexer: file too big. marking it for later proccessing");
        too_big_files.push_back(dir_entry.path());
        continue;
      }
      if (threads_to_use == 1) {
        if (current_thread_filesize + filesize > thread_max_filesize) {
          index.add_to_disk();
          paths_size += current_thread_filesize;
          current_thread_filesize = 0;
        }
        current_thread_filesize += filesize;
        ++paths_count;

        log::write(1, "indexer(single threaded): indexing path: " +
                          dir_entry.path().string());
        PATH_ID_TYPE path_id = index.add_path(dir_entry.path());
        std::unordered_set<std::string> words_to_add =
            get_words(dir_entry.path());
        index.add_words(words_to_add, path_id);
        if (ec) {
        }

        continue;
      }
      if (current_batch_add_running != 0 && !needs_a_queue) {
        size_t i = 0;
        for (threads_jobs &job : async_awaits) {
          if (job.future.wait_for(std::chrono::nanoseconds(0)) ==
              std::future_status::ready) {
            log::write(1, "indexer: task done. combining.");
            LocalIndex task_result = job.future.get();
            index.combine(task_result);
            if (index.size() >= local_index_memory) {
              log::write(2, "indexer: exceeded local index memory limit. "
                            "writing to disk.");
              index.add_to_disk();
            }
            paths_size += batch_queue_added_size[job.queue_id];
            paths_count += queue[job.queue_id].size();
            batch_queue_added_size[job.queue_id] = 0;
            queue[job.queue_id].clear();
            async_awaits.erase(async_awaits.begin() + i);
            --i;
            --current_batch_add_running;
            break;
          }
          ++i;
        }
      }
      if (current_batch_add_running != threads_to_use - 1 && !needs_a_queue) {
        for (size_t i = current_batch_add_running; i < threads_to_use - 1;
             ++i) {
          if (queue.size() <= current_batch_add_next + threads_to_use) {
            needs_a_queue = true;
            break;
          }; // if no availible queues, no adding.
          log::write(1,
                     "indexer: not used up threads slot. creating a new one.");
          async_awaits.emplace_back(threads_jobs{
              std::async(std::launch::async,
                         [&queue, current_batch_add_next]() {
                           return thread_task(queue[current_batch_add_next]);
                         }),
              current_batch_add_next});
          ++current_batch_add_next;
          ++current_batch_add_running;
        }
      }

      if (!queue_has_place(batch_queue_added_size, filesize,
                           thread_max_filesize, batch_queue_add_start)) {
        for (size_t i = 0; threads_to_use > i; ++i) {
          batch_queue_added_size.push_back(0);
          queue.push_back({});
          ++batch_queue_add_start;
        }
        needs_a_queue = false;
        batch_queue_current = 0;
      }
      bool added = false;
      while (!added) {
        if (batch_queue_added_size[batch_queue_add_start +
                                   batch_queue_current] +
                filesize <=
            thread_max_filesize) {

          queue[batch_queue_add_start + batch_queue_current].push_back(
              dir_entry.path());
          batch_queue_added_size[batch_queue_add_start + batch_queue_current] +=
              filesize;
          added = true;
        }
        if (batch_queue_current + 1 == threads_to_use) {
          batch_queue_current = 0;
        } else {
          ++batch_queue_current;
        }
      }
    }
  }
  if (threads_to_use != 1) {
    bool all_done = false;
    while (!all_done) {
      if (current_batch_add_running != 0) {
        size_t i = 0;
        for (threads_jobs &job : async_awaits) {
          if (job.future.wait_for(std::chrono::nanoseconds(0)) ==
              std::future_status::ready) {
            log::write(1, "indexer: task done. combining.");
            LocalIndex task_result = job.future.get();
            index.combine(task_result);
            if (index.size() >= local_index_memory) {
              log::write(2, "indexer: exceeded local index memory limit. "
                            "writing to disk.");
              index.add_to_disk();
            }
            paths_size += batch_queue_added_size[job.queue_id];
            paths_count += queue[job.queue_id].size();
            batch_queue_added_size[job.queue_id] = 0;
            queue[job.queue_id].clear();
            async_awaits.erase(async_awaits.begin() + i);
            --i;
            --current_batch_add_running;
            break;
          }
          ++i;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(
            50)); // to not utilize 100% cpu in the main process.
      }
      if (current_batch_add_running != threads_to_use) {
        for (size_t i = current_batch_add_running; i < threads_to_use; ++i) {
          if (queue.size() <= current_batch_add_next) {
            if (current_batch_add_running == 0)
              all_done = true;
            break;
          }
          log::write(1,
                     "indexer: not used up threads slot. creating a new one.");
          async_awaits.emplace_back(threads_jobs{
              std::async(std::launch::async,
                         [&queue, current_batch_add_next]() {
                           return thread_task(queue[current_batch_add_next]);
                         }),
              current_batch_add_next});
          ++current_batch_add_next;
          ++current_batch_add_running;
        }
      }
    }
  }
  if (threads_to_use == 1) {
    paths_size += current_thread_filesize;
  }

  log::write(2, "indexer: start_from: sorting local index.");
  index.sort();
  if (ec) {
    log::write(4, "indexer: start_from: error.");
    return 1;
  }
  log::write(2, "done.");
  log::write(2, "total size in MB: " + std::to_string(index.size() / 1000000));
  log::write(2, "total indexed files: " + std::to_string(paths_count));
  log::write(2, "total indexed file size in MB: " +
                    std::to_string(paths_size / 1000000));
  log::write(2, "files too big to be indexed: " +
                    std::to_string(too_big_files.size()));
  log::write(2, "writting to disk");
  index.add_to_disk();
  return 0;
}

#include "functions.h"
#include "lib/mio.hpp"
#include "lib/english_stem.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>
#include <future>
#include <thread>

stemming::english_stem<> indexer::StemEnglish;
bool indexer::config_loaded = false;
bool indexer::scan_dot_paths = false;
std::filesystem::path indexer::path_to_scan;
int indexer::threads_to_use = 1;
size_t indexer::local_index_memory = 5000000;

void indexer::save_config(const bool config_scan_dot_paths, const std::filesystem::path& config_path_to_scan, const int config_threads_to_use, const size_t& config_local_index_memory) {
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
	if (config_local_index_memory > 5000000) { // ignore larger memory as most modern system will handle that
		local_index_memory = config_local_index_memory; 
	}
	scan_dot_paths = config_scan_dot_paths;
	path_to_scan = config_path_to_scan;
	log::write(1, "indexer: save_config: saved config successfully.");
	config_loaded = true;
	return;
}

bool indexer::extension_allowed(const std::filesystem::path& path) {
	if (path.extension() == ".txt" || path.extension() == ".md") return true;
	return false;
}

std::unordered_set<std::wstring> indexer::get_words(const std::filesystem::path& path) {
	std::error_code ec;
	std::unordered_set<std::wstring> words_return;
	mio::mmap_source file;
	file.map(path.string(), ec);
	std::wstring current_word = L"";
	for (char c : file) {
		helper::convert_char(c);
		if (c == '!') {
			if  (current_word.length() > 4 && current_word.length() < 15) {				
				StemEnglish(current_word);	
				words_return.insert(current_word);
			}
			current_word.clear();
		} else {
			current_word.push_back(c);
		}
	}
	if  (current_word.length() > 3 && current_word.length() < 20) {
                StemEnglish(current_word);
                words_return.insert(current_word);
        }
	file.unmap();
	// file name
	current_word.clear();
	for (char c : path.filename().string()) {
		helper::convert_char(c);
                if (c == '!') {
                        if  (current_word.length() > 4 && current_word.length() < 15) {
                                words_return.insert(current_word);
                        }
                        current_word.clear();
                } else {
                        current_word.push_back(c);
                }

	}
	if  (current_word.length() > 3 && current_word.length() < 20) {
                StemEnglish(current_word);
                words_return.insert(current_word);
        }

	if (ec) {
		log::write(3, "indexerer: get_words: error reading / normalizing file.");
	}
	return words_return;
}

local_index indexer::thread_task(const std::vector<std::filesystem::path> paths_to_index) {
	std::error_code ec;
	log::write(1, "indexer: thread_task: new task started.");
	local_index task_index;
	for (const std::filesystem::path& path : paths_to_index) {
		log::write(1, "indexer: indexing path: " + path.string());
		uint32_t path_id = task_index.add_path(path);
		std::unordered_set<std::wstring> words_to_add = get_words(path);
		task_index.add_words(words_to_add, path_id);
		if (ec) {

		}
	}
	return task_index;
}

void indexer::task_start(const std::vector<std::vector<std::filesystem::path>>& paths, local_index& index) {
	std::error_code ec;
	log::write(1, "indexer: task_start: starting tasks jobs.");
	std::vector<std::future<local_index>> async_awaits;
	async_awaits.reserve(threads_to_use);

	for(int i = 0; i < threads_to_use; ++i) {
		async_awaits.emplace_back(std::async(std::launch::async, 
			[&paths, i]() { return thread_task(paths[i]); }
		));
		if (ec) {

		}
	}

	for(int i = 0; i < threads_to_use; ++i) {
		log::write(1, "indexer: task_start: combining thread...");
		local_index task_result = async_awaits[i].get();
		index.combine(task_result);
	}
}

int indexer::start_from() {
	if (!config_loaded) {
		return 1;
	}
	std::error_code ec;
	local_index index;
	log::write(2, "indexer: starting scan from last successful scan.");
	std::vector<std::vector<std::filesystem::path>> queue(threads_to_use);
	std::vector<std::filesystem::path> too_big_files;
	size_t current_thread_filesize = 0;
	size_t thread_max_filesize = local_index_memory / threads_to_use;
	uint16_t thread_counter = 0;

	for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(path_to_scan, std::filesystem::directory_options::skip_permission_denied)) {
		if (extension_allowed(dir_entry.path()) && !std::filesystem::is_directory(dir_entry, ec) && dir_entry.path().string().find("/.") == std::string::npos) {	
			size_t filesize = std::filesystem::file_size(dir_entry.path(), ec);
			if (ec) continue;
			if (filesize > thread_max_filesize) {
				too_big_files.push_back(dir_entry.path());
				continue;
			}
			if (thread_counter == threads_to_use - 1 && current_thread_filesize + filesize > thread_max_filesize) { // if all paths for all threads are done, index.
				thread_counter = 0;
				current_thread_filesize = 0;
				task_start(queue, index);
				for (std::vector<std::filesystem::path>& queue_item : queue) {
					queue_item.clear();
				}
				if (local_index_memory < index.size()) {
					log::write(1, "local index memory max exceeded. writing to disk.");
					index.add_to_disk();
				}
			} else if (current_thread_filesize + filesize > thread_max_filesize) {
				++thread_counter;
				current_thread_filesize = filesize;
				queue[thread_counter].push_back(dir_entry.path());
				continue;
			}
			current_thread_filesize += filesize;
			queue[thread_counter].push_back(dir_entry.path());
		}
	}
	task_start(queue, index);

	log::write(2, "indexer: start_from: sorting local index.");
	index.sort();
	if (ec) {
		log::write(4, "indexer: start_from: error.");
		return 1;
	}
	log::write(2, "done.");
	log::write(2, "total size in MB: " + std::to_string(index.size() / 1000000));
	log::write(2, "writting to disk");
        index.add_to_disk();
	return 0;
}

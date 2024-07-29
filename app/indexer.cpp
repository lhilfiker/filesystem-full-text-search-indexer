#include "functions.h"
#include "lib/mio.hpp"
#include "lib/english_stem.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

stemming::english_stem<> indexer::StemEnglish;
bool indexer::config_loaded = false;
bool indexer::scan_dot_paths = false;
std::filesystem::path indexer::path_to_scan;

void indexer::save_config(bool config_scan_dot_paths, std::filesystem::path config_path_to_scan) {
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

int indexer::start_from(const std::filesystem::file_time_type& from_time) {
	if (!config_loaded) {
		return 1;
	}
	std::error_code ec;
	local_index index;
	log::write(2, "indexer: starting scan from last successful scan.");
	for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(path_to_scan, std::filesystem::directory_options::skip_permission_denied)) {
		if (extension_allowed(dir_entry.path()) && !std::filesystem::is_directory(dir_entry, ec) && dir_entry.path().string().find("/.") == std::string::npos) {
			log::write(1, "indexer: start_from: indexing path: " + dir_entry.path().string());
			uint32_t path_id = index.add_path(dir_entry.path());
			std::unordered_set<std::wstring> words_to_add = get_words(dir_entry.path());
			index.add_words(words_to_add, path_id);
		}
	}
	log::write(2, "indexer: start_from: sorting local index.");
	index.sort();
	if (ec) {
		log::write(4, "indexer: start_from: error.");
		return 1;
	}
	return 0;
}

#include "functions.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

bool config_loaded = false;
bool indexer::scan_dot_paths = false;
std::filesystem::path indexer::path_to_scan;

void indexer::save_config(bool config_scan_dot_paths, std::filesystem::path config_path_to_scan) {
	scan_dot_paths = config_scan_dot_paths;
	path_to_scan = config_path_to_scan;
	log::write(1, "indexer: save_config: saved config successfully.");
	config_loaded = true;
	return;
}

bool extension_allowed(const std::filesystem::path& path) {
	if (path.extension() == ".txt") return true;
	return false;
}

std::unordered_set<std::wstring> indexer::get_words(const std::filesystem::path& path) {
	// memory map files, read chars, stopwors, stem word, etc...
}

int indexer::start_from(const std::filesystem::file_time_type& from_time) {
	std::error_code ec;
	log::write(2, "indexer: starting scan from last successful scan.");
	local_index local_reversed_index;

	for (const auto& dir_entry : std::filesystem::recursive_directory_iterator(path_to_scan, std::filesystem::directory_options::skip_permission_denied)) {
		if (extension_allowed(dir_entry.path()) && !std::filesystem::is_directory(dir_entry, ec) && dir_entry.path().string().find("/.") == std::string::npos) {
			uint32_t path_id = local_reversed_index::add_path(dir_entry.path());
			local_reversed_index::add_words(, path_id); 
		}
	}

	if (ec) {
		log::write(4, "indexer: start_from: error.");
		return 1;
	}
	return 0;
}

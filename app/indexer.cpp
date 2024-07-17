#include "functions.h"
#include <string>
#include <vector>
#include <unordered_set>

bool indexer::scan_dot_paths = false;

void indexer::save_config(bool config_scan_dot_paths) {
	scan_dot_paths = config_scan_dot_paths;
	log::write(1, "indexer: save_config: saved config successfully.");
	return;
}

int indexer::start_from(const std::filesystem::file_time_type& from_time) {
	std::error_code ec;
	log::write(2, "indexer: starting scan from last successful scan.");
	local_index local_reversed_index;


	if (ec) {
		log::write(4, "indexer: start_from: error.");
		return 1;
	}
	return 0;
}

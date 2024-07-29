#include "lib/mio.hpp"
#include "functions.h"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>

bool index::is_config_loaded = false;
bool index::is_mapped = false;
bool index::first_time = false;
int index::buffer_size = 0;
std::filesystem::path index::index_path;
int index::paths_size = 0;
int index::paths_size_buffer = 0;
int index::words_size = 0;
int index::words_size_buffer = 0;
int index::words_f_size = 0;
int index::words_f_size_buffer = 0;
int index::reversed_size = 0;
int index::reversed_size_buffer = 0;
int index::additional_size = 0;
int index::additional_size_buffer = 0;
mio::mmap_sink index::mmap_paths;
mio::mmap_sink index::mmap_words;
mio::mmap_sink index::mmap_words_f;
mio::mmap_sink index::mmap_reversed;
mio::mmap_sink index::mmap_additional;

void index::save_config(const std::filesystem::path& config_index_path, int config_buffer_size) {
	index_path = config_index_path;
	buffer_size = config_buffer_size;
	if (buffer_size < 10000) {
		buffer_size = 10000;
		log::write(3, "index: save_config: buffer size needs to be atleast 10000. setting it to 10000.");
	}
	if (buffer_size > 10000000) {
		buffer_size = 10000000;
		log::write(3, "index: save_config: buffer size can not be larger than ~10MB, setting it to ~10MB");
	}
	is_config_loaded = true;
	log::write(1, "index: save_config: saved config successfully.");
	return;
}

bool index::is_index_mapped() {
	return is_mapped;
}

void index::resize(const std::filesystem::path& path_to_resize, const int size) {
	std::error_code ec;
	std::filesystem::resize_file(path_to_resize, size);
	if (ec) {
		log::error("index: resize: could not resize file at path: " + (path_to_resize).string() + " with size: " + std::to_string(size) + ".");
	}
	log::write(1, "indexer: resize: resized file successfully.");
	return;
}

int index::unmap() {
	std::error_code ec;
	if (mmap_paths.is_mapped()) {
		mmap_paths.unmap();
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
		log::write(4, "index: unmap: error when unmapping index files.");
		return 1;
	}
	log::write(1, "index: unmap: unmapped all successfully.");
	return 0;
}

int index::map() {
	std::error_code ec;
	if (!mmap_paths.is_mapped()) {
		mmap_paths = mio::make_mmap_sink((index_path / "paths.index").string(), 0, mio::map_entire_file, ec);
	}
	if (!mmap_words.is_mapped()) {
		mmap_words = mio::make_mmap_sink((index_path / "words.index").string(), 0, mio::map_entire_file, ec);
	}
	if (!mmap_words_f.is_mapped()) {
		mmap_words_f = mio::make_mmap_sink((index_path / "words_f.index").string(), 0, mio::map_entire_file, ec);
	}
	if (!mmap_reversed.is_mapped()) {
		mmap_reversed = mio::make_mmap_sink((index_path / "reversed.index").string(), 0, mio::map_entire_file, ec);
	}
	if (!mmap_additional.is_mapped()) {
		mmap_additional = mio::make_mmap_sink((index_path / "additional.index").string(), 0, mio::map_entire_file, ec);
	}
	if (ec) {
		log::write(3, "index: map: error when mapping index files.");
		return 1;
	}
	log::write(1, "index: map: mapped all successfully.");
	return 0;
}

int index::get_actual_size(const mio::mmap_sink& mmap) {
	std::error_code ec;
	if (!mmap.is_mapped()) {
		log::write(3, "index: get_actual_size: not mapped. can not count.");
		return -1;
	}
	int size = 0;
	for (const char& c: mmap) {
		if (c == '\0') break;
		++size;
	}
	if (ec) {
		log::write(3, "index: get_actual_size: error when couting actual size");
		return -1;
	}
	log::write(1, "index: get_actual_size: successfully counted size: " + std::to_string(size));
	return size;
}

void index::check_files() {
	if (!is_config_loaded) {
		log::error("index: check_files: config not loaded. can not continue.");
	}
	log::write(1, "index: check_files: checking files.");
	std::error_code ec;
	if (!std::filesystem::is_directory(index_path)) {
		log::write(1, "index: check_files: creating index directory.");
		std::filesystem::create_directories(index_path);
	}
	if (!std::filesystem::exists(index_path / "paths.index") || !std::filesystem::exists(index_path / "words.index") || !std::filesystem::exists(index_path / "words_f.index") || !std::filesystem::exists(index_path / "reversed.index") || !std::filesystem::exists(index_path / "additional.index") (words_size == 0 || words_f_size == 0 || paths_size == 0 || reversed_size == 0 || additional_size == 0) {
		first_time = true;
		log::write(1, "index: check_files: index files damaged / not existing, recreating.");
		std::ofstream { index_path / "paths.index" };
		std::ofstream { index_path / "words.index" };
		std::ofstream { index_path / "words_f.index" };
		std::ofstream { index_path / "reversed.index" };
		std::ofstream { index_path / "additional.index" };
	}
	if (ec) {
		log::error("index: check_files: error accessing/creating index files in " + (index_path).string() + ".");
	}
	return;
}

int index::initialize() {
	if (!is_config_loaded) {
		log::write(4, "index: initialize: config not loaded. can not continue.");
		return 1;
	}
	is_mapped = false;
	std::error_code ec;
	unmap(); //unmap anyway incase they are already mapped.
	check_files(); // check if index files exist and create them.
	map(); // ignore error here as it might fail if file size is 0.
	// get actual sizes of the files to reset buffer.
	if (int paths_size = get_actual_size(mmap_paths); paths_size == -1 && helper::file_size(index_path / "paths.index") > 0) paths_size = 0;	
	if (int words_size = get_actual_size(mmap_words); words_size == -1 && helper::file_size(index_path / "words.index") > 0) words_size = 0;
	if (int words_f_size = get_actual_size(mmap_words_f); words_f_size == -1 && helper::file_size(index_path / "words_f.index") > 0) words_f_size = 0;	
	if (int reversed_size = get_actual_size(mmap_reversed); reversed_size == -1 && helper::file_size(index_path / "reversed.index") > 0) reversed_size = 0;
	if (int additional_size = get_actual_size(mmap_additional); additional_size == -1 && helper::file_size(index_path / "additional.index") > 0) additional_size = 0;
	if ( paths_size == -1 || words_size == -1 || words_f_size == -1 || reversed_size == -1 || additional_size == -1) {
		log::write(4, "index: initialize: could not get actual size of index files, exiting.");
		return 1;
	}
	paths_size_buffer = paths_size + buffer_size;
	words_size_buffer = words_size + buffer_size;
	words_f_size_buffer = words_f_size + buffer_size;
	reversed_size_buffer = reversed_size + buffer_size;
	additional_size_buffer = additional_size + buffer_size;
	if (unmap() == 1) { // unmap to resize
		log::write(4, "index: initialize: could not unmap, see above log message, exiting.");
		return 1;
	}
	//resize actual size + buffer size
	resize(index_path / "paths.index", paths_size_buffer);
	resize(index_path / "words.index", words_size_buffer);
	resize(index_path / "words_f.index", words_f_size_buffer);
	resize(index_path / "reversed.index", reversed_size_buffer);
	resize(index_path / "additional.index", additional_size_buffer);
	// map after resizing
	if (map() == 1) { // map files
		log::write(4, "index: initialize: could not map, see above log message, exiting.");
		return 1;
	}
	is_mapped = true;
	return 0;
}

int index::uninitialize() {
	if (!is_config_loaded) {
		log::write(4, "index: uninitialize: config not loaded. can not continue.");
		return 1;
	}
	is_mapped = false;
	// write caches, etc...
	if (unmap() == 1) {
		log::write(4, "index: uninitialize: could not unmap.");
		return 1;
	}
	return 0;
}

int index::add(const std::vector<std::string>& paths, std::vector<words_reversed>& words_reversed) {
	std::error_code ec;
	if (first_time) {
	// empty, first time write
	} else {
	//compare and add
	}

}

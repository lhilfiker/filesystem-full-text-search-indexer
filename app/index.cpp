#include "lib/mio.hpp"
#include "functions.h"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>

bool index::is_config_loaded = false;
bool index::is_mapped = false;
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

void index::save_config(std::filesystem::path config_index_path, int config_buffer_size) {
	index_path = config_index_path;
	buffer_size = config_buffer_size;
	if (buffer_size < 10000) {
		buffer_size = 10000;
		log::write(3, "index: save_config: buffer size needs to be atleast 10000. setting it to 10000.");
	}
	is_config_loaded = true;
	return;
}

int index::resize(std::filesystem::path path_to_resize, int size) {
	std::error_code ec;
	std::filesystem::resize_file(path_to_resize, size);
	if (ec) {
		log::write(4, "index: resize: could not resize file at path: " + (path_to_resize).string() + " with size: " + std::to_string(size) + ".");
		return 1;
	}
	return 0;
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
		log::write(4, "index: map: error when mapping index files.");
		return 1;
	}
	return 0;
}

int index::get_actual_size(const mio::mmap_sink& mmap) {
	std::error_code ec;
	int size = 0;
	for (const char& c: mmap) {
		if (c == '\0') break;
		++size;
	}
	if (ec) {
		log::write(4, "index: get_actual_size: error when couting actual size");
		return -1;
	}
	log::write(1, "index: get_actual_size: successfully counted size: " + std::to_string(size));
	return size;
}

int index::check_files() {
	if (!is_config_loaded) {
		log::write(4, "index: check_files: config not loaded. can not continue.");
		return 1;
	}
	log::write(1, "index: check_files: checking files.");
	std::error_code ec;
	if (!std::filesystem::is_directory(index_path)) {
		log::write(1, "index: check_files: creating index directory.");
		std::filesystem::create_directories(index_path);
	}
	if (!std::filesystem::exists(index_path / "paths.index") || !std::filesystem::exists(index_path / "words.index") || !std::filesystem::exists(index_path / "words_f.index") || !std::filesystem::exists(index_path / "reversed.index") || !std::filesystem::exists(index_path / "additional.index")) {
		log::write(1, "index: check_files: index files damaged / not existing, recreating.");
		std::ofstream { index_path / "paths.index" };
		resize(index_path / "paths.index", 10); // a hacky fix for when index files sizes is empty and mio can memory map them.
		std::ofstream { index_path / "words.index" };
		resize(index_path / "words.index", 10);
		std::ofstream { index_path / "words_f.index" };
		resize(index_path / "words_f.index", 10);
		std::ofstream { index_path / "reversed.index" };
		resize(index_path / "reversed.index", 10);
		std::ofstream { index_path / "additional.index" };
		resize(index_path / "additional.index", 10);
	}
	if (ec) {
		log::write(4, "index: check_files: error accessing/creating index files in " + (index_path).string() + ".");
		return 1;
	}
	return 0;
}

int index::initialize() {
	if (!is_config_loaded) {
		log::write(4, "index: initialize: config not loaded. can not continue.");
		return 1;
	}
	is_mapped = false;
	std::error_code ec;
	unmap(); //unmap anyway incase they are already mapped.
	if(check_files() == 1) { // check if index files exist and create them.
		log::write(4, "index: initialize: could not check_files, see above log message, exiting.");
		return 1;
	}
	if (map() == 1) { // map files
		log::write(4, "index: initialize: could not map, see above log message, exiting.");
		return 1;
	}
	// get actual sizes of the files to reset buffer.
	paths_size = get_actual_size(mmap_paths); 
	paths_size_buffer = paths_size + buffer_size;
	words_size = get_actual_size(mmap_words);
	words_size_buffer = words_size + buffer_size;
	words_f_size = get_actual_size(mmap_words_f);
	words_f_size_buffer = words_f_size + buffer_size;
	reversed_size = get_actual_size(mmap_reversed); 
	reversed_size_buffer = reversed_size + buffer_size;
	additional_size = get_actual_size(mmap_additional);
	additional_size_buffer = additional_size + buffer_size;
	if ( paths_size == -1 || words_size == -1 || words_f_size == -1 || reversed_size == -1 || additional_size == -1 ) {
		log::write(4, "index: initialize: could not get actual size of index files, exiting.");
		return 1;
	}
	if (unmap() == 1) { // unmap to resize
		log::write(4, "index: initialize: could not unmap, see above log message, exiting.");
		return 1;
	}
	//resize actual size + buffer size
	if (resize(index_path / "paths.index", paths_size_buffer) == 1) {
		log::write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path / "words.index", words_size_buffer) == 1) {
		log::write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path / "words_f.index", words_f_size_buffer) == 1) {
		log::write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path / "reversed.index", reversed_size_buffer) == 1) {
		log::write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path / "additional.index", additional_size_buffer) == 1) {
		log::write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
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
	// unmapp, write caches, etc...
	return 0;
}

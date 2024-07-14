#include "lib/mio.hpp"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>

class index {
	private:
		static bool is_mapped = false;
		static int buffer_size = 0;
		static std::filesystem::path index_path;
		static int paths_size = 0;
		static int paths_size_buffer = 0;
		static int words_size = 0;
		static int words_size_buffer = 0;
		static int words_f_size = 0;
		static int words_f_size_buffer = 0;
		static int reversed_size = 0;
		static int reversed_size_buffer = 0;
		static int additional_size = 0;
		static int additional_size_buffer = 0;
		static mio::mmap_sink mmap_paths;
		static mio::mmap_sink mmap_words;
		static mio::mmap_sink mmap_words_f;
		static mio::mmap_sink mmap_reversed;
		static mio::mmap_sink mmap_additional;
		int check_files();
		int get_actual_size(const mio::mmap_sink& mmap);
		int map();
		int unmap();
		int resize(std::filesystem::path path_to_resize, int size);
	public:
		void save_config(std::filesystem::path config_index_path, int config_buffer_size);
		int initialize();
		int uninitialize();
};

void index::save_config(std::filesystem::path config_index_path, int config_buffer_size) {
	index_path = config_index_path;
	buffer_size = config_buffer_size;
	if (buffer_size < 10000) {
		buffer_size = 10000;
		log.write(3, "index: save_config: buffer size needs to be atleast 10000. setting it to 10000.");
	}
	return;
}

int index::resize(std::filesystem::path path_to_resize, int size) {
	std::error_code ec;
	std::filesystem::resize_file(path_to_resize, int size);
	if (ec) {
		log.write(4, "index: resize: could not resize file at path: " + std::to_string(path_to_resize) + " with size: " + size + ".");
		return 1;
	}
	return 0;
}

int index::unmap() {
	std::error_code ec;
	if (mmap_paths.mapped()) {
		mmap_paths.unmap();
	}
	if (mmap_words.mapped()) {
		mmap_words.unmap();
	}
	if (mmap_words_f.mapped()) {
		mmap_words_f.unmap();
	}
	if (mmap_reversed.mapped()) {
		mmap_reversed.unmap();
	}
	if (mmap_additional.mapped()) {
		mmap_additional.unmap();
	}
	if (ec) {
		log.write(4, "index: unmap: error when unmapping index files.");
		return 1;
	}
	return 0;
}

int index::map() {
	std::error_code ec;
	if (!mmap_paths.mapped()) {
		mmap_paths = mio::make_mmap_sink(std::to_string(index_path \ "paths.index"), 0, mio::map_entire_file, ec);
	}
	if (!mmap_words.mapped()) {
		mmap_words = mio::make_mmap_sink(std::to_string(index_path \ "words.index"), 0, mio::map_entire_file, ec);
	}
	if (!mmap_words_f.mapped()) {
		mmap_words_f = mio::make_mmap_sink(std::to_string(index_path \ "words_f.index"), 0, mio::map_entire_file, ec);
	}
	if (!mmap_reversed.mapped()) {
		mmap_reversed = mio::make_mmap_sink(std::to_string(index_path \ "reversed.index"), 0, mio::map_entire_file, ec);
	}
	if (!mmap_additional.mapped()) {
		mmap_additional = mio::make_mmap_sink(std::to_string(index_path \ "additional.index"), 0, mio::map_entire_file, ec);
	}
	if (ec) {
		log.write(4, "index: map: error when mapping index files.");
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
	if (ec) return -1;
	return size;
}

int index::check_files() {
	std::error_code ec;
	if (!std::filesystem::is_directory(index_path)) {
		std::filesystem::create_directories(index_path);
	}
	if (!std::filesystem::exists(index_path \ "paths.index") || !std::filesystem::exists(index_path \ "words.index") || !std::filesystem::exists(index_path \ "words_f.index") || !std::filesystem::exists(index_path \ "reversed.index") || !std::filesystem::exists(index_path \ "additional.index")) {
		std::ofstream { index_path \ "paths.index" };
		std::ofstream { index_path \ "words.index" };
		std::ofstream { index_path \ "words_f.index" };
		std::ofstream { index_path \ "reversed.index" };
		std::ofstream { index_path \ "additional.index" };
	}
	if (ec) {
		log.write(4, "index: check_files: error accessing/creating index files in " + std::to_string(index_path) + ".");
		return 1;
	}
	return 0;
}

int index::initialize() {
	is_mapped = false;
	std::error_code ec;
	unmap(); //unmap anyway incase they are already mapped.
	if(check_files() == 1) { // check if index files exist and create them.
		log.write(4, "index: initialize: could not check_files, see above log message, exiting.");
		return 1;
	}
	if (map() == 1) { // map files
		log.write(4, "index: initialize: could not map, see above log message, exiting.");
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
		log.write(4, "index: initialize: could not get actual size of index files, exiting.");
		return 1;
	}
	if (unmap() == 1) { // unmap to resize
		log.write(4, "index: initialize: could not unmap, see above log message, exiting.");
		return 1;
	}
	//resize actual size + buffer size
	if (resize(index_path \ "paths.index", paths_size_buffer) == 1) {
		log.write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path \ "words.index", words_size_buffer) == 1) {
		log.write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path \ "words_f.index", words_f_size_buffer) == 1) {
		log.write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path \ "reversed.index", reversed_size_buffer) == 1) {
		log.write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	if (resize(index_path \ "additional.index", additional_size_buffer) == 1) {
		log.write(4, "index: initialize: could not resize file, exiting.");
		return 1;
	}
	// map after resizing
	if (map() == 1) { // map files
		log.write(4, "index: initialize: could not map, see above log message, exiting.");
		return 1;
	}
	is_mapped = true;

}

int index::uninitialize() {
	// unmapp, write caches, etc...
}

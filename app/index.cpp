#include "lib/mio.hpp"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>

// config_loaded var && load_config_vals function which sets all paths. inputs index dir. removes index dir inputs in other funtions but requires this to be called first else it throws and error.

class index {
	private:
		bool is_mapped = false;
		int paths_size = 0;
		int words_size = 0;
		int words_f_size = 0;
		int reversed_size = 0;
		int additional_size = 0;
		static mio::mmap_sink mmap_paths;
		static mio::mmap_sink mmap_words;
		static mio::mmap_sink mmap_words_f;
		static mio::mmap_sink mmap_reversed;
		static mio::mmap_sink mmap_additional;
		int check_files(std::filesystem::path dir_path);
		int get_actual_size(const mio::mmap_sink& mmap);
	public:
		int initialize(std::filesystem::path index_path, int buffer_size);
		int unmap(std::filesystem::path index_path);
};

int unmap(std::filesystem::path index_path) {
 // unmap
}

int get_actual_size(const mio::mmap_sink& mmap) {
	std::error_code ec;
	int size = 0;
	for (const char& c: mmap) {
		if (c == '\0') break;
		++size;
	}
	if (ec) return -1;
	return size;
}

int index::check_files(std::filesystem::path dir_path) {
	std::error_code ec;
	if (!std::filesystem::is_directory(dir_path)) {
		std::filesystem::create_directories(dir_path);
	}
	if (!std::filesystem::exists(dir_path \ "paths.index") || !std::filesystem::exists(dir_path \ "words.index") || !std::filesystem::exists(dir_path \ "words_f.index") || !std::filesystem::exists(dir_path \ "reversed.index") || !std::filesystem::exists(dir_path \ "additional.index")) {
		std::ofstream { dir_path \ "paths.index" };
		std::ofstream { dir_path \ "words.index" };
		std::ofstream { dir_path \ "words_f.index" };
		std::ofstream { dir_path \ "reversed.index" };
		std::ofstream { dir_path \ "additional.index" };
	}
	if (ec) {
		// TODO: LOG ERROR
		return 1;
	}
	return 0;
}

int index::initialize(std::filesystem::path index_path, int buffer_size) {
	is_mapped = false;
	std::error_code ec;
	if(mmap_paths.mapped() || mmap_words.mapped() || mmap_words_f.mapped() || mmap_reversed.mapped() || mmap_additional.mapped()) {
		// Unmap if trying to remapp.
		if (unmapp() == 1) {
			// TODO: LOG ERROR
			return 1;
		}
	}
	if(check_files == 1) { // check if index files exist and create them.
		//TODO: LOG ERROR
		return 1;
	}
	// get actual sizes of the files to reset buffer.
	paths_size = get_actual_size(mmap_paths); 
	words_size = get_actual_size(mmap_words); 
	words_f_size = get_actual_size(mmap_words_f); 
	reversed_size = get_actual_size(mmap_reversed); 
	additional_size = get_actual_size(mmap_additional);
	if ( paths_size == -1 || words_size == -1 || words_f_size == -1 || reversed_size == -1 || additional_size == -1 ) {
		//TODO: LOG ERROR
		return 1;
	}
}

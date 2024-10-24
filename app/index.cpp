#include "lib/mio.hpp"
#include "functions.h"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <array>
#include <bitset>

union bit_byte {
	std::bitset<8> bits;
	unsigned char all;

	bit_byte() : all(0) {}
};

union reversed_block {
	uint16_t ids [5];
	char bytes [10];
};

bool index::is_config_loaded = false;
bool index::is_mapped = false;
bool index::first_time = false;
int index::buffer_size = 0;
std::filesystem::path index::index_path;
int index::paths_size = 0;
int index::paths_size_buffer = 0;
int index::paths_count_size = 0;
int index::paths_count_size_buffer = 0;
int index::words_size = 0;
int index::words_size_buffer = 0;
int index::words_f_size = 0;
int index::words_f_size_buffer = 0;
int index::reversed_size = 0;
int index::reversed_size_buffer = 0;
int index::additional_size = 0;
int index::additional_size_buffer = 0;
mio::mmap_sink index::mmap_paths;
mio::mmap_sink index::mmap_paths_count;
mio::mmap_sink index::mmap_words;
mio::mmap_sink index::mmap_words_f;
mio::mmap_sink index::mmap_reversed;
mio::mmap_sink index::mmap_additional;

void index::save_config(const std::filesystem::path& config_index_path, const int config_buffer_size) {
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
        if (!mmap_paths_count.is_mapped()) {
                mmap_paths_count = mio::make_mmap_sink((index_path / "paths_count.index").string(), 0, mio::map_entire_file,
ec);
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
	if (!std::filesystem::exists(index_path / "paths.index") || helper::file_size(index_path / "paths.index") == 0 || !std::filesystem::exists(index_path / "paths_count.index")  || helper::file_size(index_path / "paths_count.index") == 0 || !std::filesystem::exists(index_path / "words.index")  || helper::file_size(index_path / "words.index") == 0|| !std::filesystem::exists(index_path / "words_f.index")  || helper::file_size(index_path / "words_f.index") == 0 || !std::filesystem::exists(index_path / "reversed.index")  || helper::file_size(index_path / "reversed.index") == 0 || !std::filesystem::exists(index_path / "additional.index") || helper::file_size(index_path / "additional.index") == 0) {
		first_time = true;
		log::write(1, "index: check_files: index files damaged / not existing, recreating.");
		std::ofstream { index_path / "paths.index" };
		std::ofstream { index_path / "paths_count.index" };
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
        if (int paths_count_size = get_actual_size(mmap_paths); paths_count_size == -1 && helper::file_size(index_path / "paths_count.index") > 0) paths_count_size = 0;
	if (int words_size = get_actual_size(mmap_words); words_size == -1 && helper::file_size(index_path / "words.index") > 0) words_size = 0;
	if (int words_f_size = get_actual_size(mmap_words_f); words_f_size == -1 && helper::file_size(index_path / "words_f.index") > 0) words_f_size = 0;	
	if (int reversed_size = get_actual_size(mmap_reversed); reversed_size == -1 && helper::file_size(index_path / "reversed.index") > 0) reversed_size = 0;
	if (int additional_size = get_actual_size(mmap_additional); additional_size == -1 && helper::file_size(index_path / "additional.index") > 0) additional_size = 0;
	if ( paths_size == -1 || words_size == -1 || words_f_size == -1 || reversed_size == -1 || additional_size == -1) {
		log::write(4, "index: initialize: could not get actual size of index files, exiting.");
		return 1;
	}
	paths_size_buffer = paths_size + buffer_size;
	paths_count_size_buffer = paths_count_size + buffer_size;
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
	resize(index_path / "paths_count.index", paths_count_size_buffer);
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

int index::add(std::vector<std::string>& paths, const size_t& paths_size_l, std::vector<uint32_t>& paths_count, const size_t& paths_count_size_l, std::vector<words_reversed>& words_reversed_l, const size_t& words_size_l, const size_t& reversed_size_l) {
	std::error_code ec;
	if (first_time) {
		log::write(2, "index: add: first time write.");
		// paths
		uint64_t file_location = 0;
		paths_size_buffer = paths.size() + paths_size_l;	
		paths_count_size_buffer = paths_count_size_l;
		words_size_buffer = (((words_size_l + words_reversed_l.size()) * 5) / 8) + 5; // we save one char  as 5 bits instead of 8 (1 byte) + place for end of file char and buffer.
		// words_f_size maybe needs to be extended to allow larger numbers if 8 bytes turn out to be too small. maybe automaticly resize if running out of space?
		words_f_size_buffer = 26 * 8; // uint64_t stored as 8 bytes(64 bits) for each letter in the alphabet.
		reversed_size_buffer = words_reversed_l.size() * 10; // each word id has a 10 byte block.
		additional_size_buffer = 0;
		for (const words_reversed& additional_reversed_counter : words_reversed_l) {
			if (additional_reversed_counter.reversed.size() < 5) continue; // if under 4 words, no additional is requiered. 
			additional_size_buffer += (additional_reversed_counter.reversed.size() + 19) / 24;
		}
		additional_size_buffer *= 5;
		unmap();
		resize(index_path / "paths.index", paths_size_buffer);	
		resize(index_path / "paths_count.index", paths_count_size_buffer);
		resize(index_path / "words.index", words_size_buffer);
		resize(index_path / "words_f.index", words_f_size_buffer);
		resize(index_path / "reversed.index", reversed_size_buffer);
		resize(index_path / "additional.index", additional_size_buffer);
		map();
		for (const std::string& path : paths) {
			for (const char& c : path) {
				mmap_paths[file_location] = c;
				++file_location;
			}
			mmap_paths[file_location] = '\n';
			++file_location;
		}
		paths_size = file_location - 1; // -1 to remove the last newline character
		paths.clear(); // free memory

		log::write(2, "index: add: paths written.");
		//paths_count
		file_location = 0;
		for (const uint32_t& path_count : paths_count) {
			mmap_paths_count[file_location]     = (path_count >> 24) & 0xFF;
			mmap_paths_count[file_location + 1] = (path_count >> 16) & 0xFF;
			mmap_paths_count[file_location + 2] = (path_count >> 8)  & 0xFF;
			mmap_paths_count[file_location + 3] = path_count & 0xFF;
			file_location += 4;
		}
		paths_count_size = file_location;
		paths_count.clear(); // free memory
		
		log::write(2, "index: add: paths_count written.");
		// words & words_f & reversed
		std::array<uint64_t, 26> words_f;
		char current_char = '0';
		uint32_t current_word = 0;
		file_location = 0;
		// needed for words_f as it saves a letter as 5 bits instead of 8.
		uint64_t file_five_bit_location = 0;
		bit_byte current_byte;
		int bit_count = 7;
		for (const words_reversed& word : words_reversed_l ) {
			// if a word with a new letter comes, add its location to words_f
			if (word.word[0] != current_char) {
				current_char = word.word[0];
				words_f[current_char - 'a'] = file_five_bit_location;
			}
			std::vector<bool> all_bits;
			all_bits.reserve(word.word.length() * 5);
			for (const char c: word.word) {

				unsigned char value = c - 'a';
				std::bitset<5> bits(value);
				int word_bit_count = 5;
				while (0 < word_bit_count) {
					current_byte.bits[bit_count] = bits[word_bit_count];
					if (bit_count == 0) {
						mmap_words[file_location] = current_byte.all;
						++file_location;
						bit_count = 7;
						current_byte.all = 0;
					} else {
						--bit_count;
					}
					--word_bit_count;
				}
				++file_five_bit_location;
			}
			//insert new word char
			std::bitset<5> bits(29);
			int word_bit_count = 5;
			while (0 < word_bit_count) {
				current_byte.bits[bit_count] = bits[word_bit_count];
				if (bit_count == 0) {
					mmap_words[file_location] = current_byte.all;
					++file_location;
					bit_count = 7;
					current_byte.all = 0;
				} else {
					--bit_count;
				}
				--word_bit_count;
			}
			++file_five_bit_location;
			++current_word;
		}
		// write rest the remaining byte if needed and then write the end of file char (30)
		std::bitset<5> bits(30);
		int word_bit_count = 5;
		while (0 < word_bit_count) {
			current_byte.bits[bit_count] = bits[word_bit_count];
			if (bit_count == 0) {
				mmap_words[file_location] = current_byte.all;
				++file_location;
				bit_count = 7;
				current_byte.all = 0;
			} else {
				--bit_count;
			}
			--word_bit_count;
		}
		if (bit_count != 7) {
			mmap_words[file_location] = current_byte.all;
			++file_location;
		}
		file_five_bit_location = 0;
		words_size = file_location;
		log::write(2, "indexer: add: words written");
		
		// write words_f
		file_location = 0;
                for (const uint64_t& word_start_f : words_f) {
                        mmap_words_f[file_location]     = (word_start_f >> 56) & 0xFF;
                        mmap_words_f[file_location + 1] = (word_start_f >> 48) & 0xFF;
                        mmap_words_f[file_location + 2] = (word_start_f >> 40)  & 0xFF; 
                        mmap_words_f[file_location + 3] = (word_start_f >> 32)  & 0xFF;
                        mmap_words_f[file_location + 4] = (word_start_f >> 24)  & 0xFF;
                        mmap_words_f[file_location + 5] = (word_start_f >> 16)  & 0xFF;
                        mmap_words_f[file_location + 6] = (word_start_f >> 8)  & 0xFF;
			mmap_words_f[file_location + 7] = word_start_f & 0xFF;
                        file_location += 8;
                }
		words_f_size = file_location;
		log::write(2, "indexer: add: words_f written");
		// reversed & additional
		file_location = 0;
		size_t additional_id = 1;
		for (const words_reversed& reversed : words_reversed_l) {
			// it just needs a reversed block and no additional.
			reversed_block current_reversed_block{};
			if (reversed.reversed.size() <= 4) {
				for (int i = 0; i < reversed.reversed.size(); ++i) {
					current_reversed_block.ids[i] = reversed.reversed[i] + 1; //paths are indexed from 1 because 0 is reserved for empty values.	
				}
			}

			// write reversed block
			for (int i = 0; i < 10; ++i) {
				mmap_reversed[file_location] = current_reversed_block.bytes[i];
				++file_location;
			}
		}
		reversed_size = file_location;

		unmap();
                resize(index_path / "paths_count.index", paths_count_size);
                resize(index_path / "paths.index", paths_size);
		resize(index_path / "words.index", words_size);
		resize(index_path / "words_f.index", words_f_size);
		resize(index_path / "reversed.index", reversed_size);
		map();
		first_time = false;
		
		if (ec) {
			log::write(3, "index: add: error");
			return 1;
		}
	} else {
		log::write(2, "indexer: add: adding to existing index.");
	}
	return 0;
}

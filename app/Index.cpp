#include "lib/mio.hpp"
#include "functions.h"
#include <string>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <array>
#include <bitset>

union BitByte {
	std::bitset<8> bits;
	unsigned char all;

	BitByte() : all(0) {}
};

union ReversedBlock {
	uint16_t ids [5];
	char bytes [10];
};

union AdditionalBlock {
	uint16_t ids[25];
	char bytes[50];
};

union PathOffset {
	uint16_t offset;
	char bytes[2];
};

union TransactionHeader {
	struct {
		uint8_t status;
		uint8_t index_type; // paths = 0, words = 1, words_f = 2, reversed = 3, additional = 4
		uint64_t location; // byte count in file(or bit for words)
		uint64_t backup_id; // 0 = none
		uint8_t operation_type; // 0 = Move, 1 = write
		uint64_t content_length; // length of content
	};
	char bytes[15];
};

struct Transaction {
	TransactionHeader header;
	std::string content;
};

union InsertionHeader {
	struct {
		uint8_t index_type;
		uint64_t location;
		uint64_t content_length;
	};
	char bytes[9];
};

struct Insertion {
	InsertionHeader header;
	std::string content;
};

bool Index::is_config_loaded = false;
bool Index::is_mapped = false;
bool Index::first_time = false;
int Index::buffer_size = 0;
std::filesystem::path Index::index_path;
int Index::paths_size = 0;
int Index::paths_size_buffer = 0;
int Index::paths_count_size = 0;
int Index::paths_count_size_buffer = 0;
int Index::words_size = 0;
int Index::words_size_buffer = 0;
int Index::words_f_size = 0;
int Index::words_f_size_buffer = 0;
int Index::reversed_size = 0;
int Index::reversed_size_buffer = 0;
int Index::additional_size = 0;
int Index::additional_size_buffer = 0;
mio::mmap_sink Index::mmap_paths;
mio::mmap_sink Index::mmap_paths_count;
mio::mmap_sink Index::mmap_words;
mio::mmap_sink Index::mmap_words_f;
mio::mmap_sink Index::mmap_reversed;
mio::mmap_sink Index::mmap_additional;

void Index::save_config(const std::filesystem::path& config_index_path, const int config_buffer_size) {
	index_path = config_index_path;
	buffer_size = config_buffer_size;
	if (buffer_size < 10000) {
		buffer_size = 10000;
		log::write(3, "Index: save_config: buffer size needs to be atleast 10000. setting it to 10000.");
	}
	if (buffer_size > 10000000) {
		buffer_size = 10000000;
		log::write(3, "Index: save_config: buffer size can not be larger than ~10MB, setting it to ~10MB");
	}
	is_config_loaded = true;
	log::write(1, "Index: save_config: saved config successfully.");
	return;
}

bool Index::is_index_mapped() {
	return is_mapped;
}

void Index::resize(const std::filesystem::path& path_to_resize, const int size) {
	std::error_code ec;
	std::filesystem::resize_file(path_to_resize, size);
	if (ec) {
		log::error("Index: resize: could not resize file at path: " + (path_to_resize).string() + " with size: " + std::to_string(size) + ".");
	}
	log::write(1, "indexer: resize: resized file successfully.");
	return;
}

int Index::unmap() {
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
		log::write(4, "Index: unmap: error when unmapping index files.");
		return 1;
	}
	log::write(1, "Index: unmap: unmapped all successfully.");
	return 0;
}

int Index::map() {
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
		log::write(3, "Index: map: error when mapping index files.");
		return 1;
	}
	log::write(1, "Index: map: mapped all successfully.");
	return 0;
}

int Index::get_actual_size(const mio::mmap_sink& mmap) {
	std::error_code ec;
	if (!mmap.is_mapped()) {
		log::write(3, "Index: get_actual_size: not mapped. can not count.");
		return -1;
	}
	int size = 0;
	for (const char& c: mmap) {
		if (c == '\0') break;
		++size;
	}
	if (ec) {
		log::write(3, "Index: get_actual_size: error when couting actual size");
		return -1;
	}
	log::write(1, "Index: get_actual_size: successfully counted size: " + std::to_string(size));
	return size;
}

void Index::check_files() {
	if (!is_config_loaded) {
		log::error("Index: check_files: config not loaded. can not continue.");
	}
	log::write(1, "Index: check_files: checking files.");
	std::error_code ec;
	if (!std::filesystem::is_directory(index_path)) {
		log::write(1, "Index: check_files: creating index directory.");
		std::filesystem::create_directories(index_path);
	}
	if (!std::filesystem::exists(index_path / "paths.index") || helper::file_size(index_path / "paths.index") == 0 || !std::filesystem::exists(index_path / "paths_count.index")  || helper::file_size(index_path / "paths_count.index") == 0 || !std::filesystem::exists(index_path / "words.index")  || helper::file_size(index_path / "words.index") == 0|| !std::filesystem::exists(index_path / "words_f.index")  || helper::file_size(index_path / "words_f.index") == 0 || !std::filesystem::exists(index_path / "reversed.index")  || helper::file_size(index_path / "reversed.index") == 0 || !std::filesystem::exists(index_path / "additional.index") || helper::file_size(index_path / "additional.index") == 0) {
		first_time = true;
		log::write(1, "Index: check_files: index files damaged / not existing, recreating.");
		std::ofstream { index_path / "paths.index" };
		std::ofstream { index_path / "paths_count.index" };
		std::ofstream { index_path / "words.index" };
		std::ofstream { index_path / "words_f.index" };
		std::ofstream { index_path / "reversed.index" };
		std::ofstream { index_path / "additional.index" };
	}
	if (ec) {
		log::error("Index: check_files: error accessing/creating index files in " + (index_path).string() + ".");
	}
	return;
}

int Index::initialize() {
	if (!is_config_loaded) {
		log::write(4, "Index: initialize: config not loaded. can not continue.");
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
		log::write(4, "Index: initialize: could not get actual size of index files, exiting.");
		return 1;
	}
	paths_size_buffer = paths_size + buffer_size;
	paths_count_size_buffer = paths_count_size + buffer_size;
	words_size_buffer = words_size + buffer_size;
	words_f_size_buffer = words_f_size + buffer_size;
	reversed_size_buffer = reversed_size + buffer_size;
	additional_size_buffer = additional_size + buffer_size;
	if (unmap() == 1) { // unmap to resize
		log::write(4, "Index: initialize: could not unmap, see above log message, exiting.");
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
		log::write(4, "Index: initialize: could not map, see above log message, exiting.");
		return 1;
	}
	is_mapped = true;
	return 0;
}

int Index::uninitialize() {
	if (!is_config_loaded) {
		log::write(4, "Index: uninitialize: config not loaded. can not continue.");
		return 1;
	}
	is_mapped = false;
	// write caches, etc...
	if (unmap() == 1) {
		log::write(4, "Index: uninitialize: could not unmap.");
		return 1;
	}
	return 0;
}

int Index::add(std::vector<std::string>& paths, const size_t& paths_size_l, std::vector<uint32_t>& paths_count, const size_t& paths_count_size_l, std::vector<words_reversed>& words_reversed_l, const size_t& words_size_l, const size_t& reversed_size_l) {
	std::error_code ec;
	if (first_time) {
		log::write(2, "Index: add: first time write.");
		// paths
		uint64_t file_location = 0;
		paths_size_buffer = (paths.size() * 2) + paths_size_l;	
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
		additional_size_buffer *= 50;
		unmap();
		resize(index_path / "paths.index", paths_size_buffer);	
		resize(index_path / "paths_count.index", paths_count_size_buffer);
		resize(index_path / "words.index", words_size_buffer);
		resize(index_path / "words_f.index", words_f_size_buffer);
		resize(index_path / "reversed.index", reversed_size_buffer);
		resize(index_path / "additional.index", additional_size_buffer);
		map();
		for (const std::string& path : paths) {
			PathOffset offset = {};
			offset.offset = path.length();
			mmap_paths[file_location] = offset.bytes[0];
			++file_location;
			mmap_paths[file_location] = offset.bytes[1];
			++file_location;
			for (const char& c : path) {
				mmap_paths[file_location] = c;
				++file_location;
			}
		}
		paths_size = file_location; // -1 to remove the last newline character
		paths.clear(); // free memory

		log::write(2, "Index: add: paths written.");
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
		
		log::write(2, "Index: add: paths_count written.");
		// words & words_f & reversed
		std::array<uint64_t, 26> words_f;
		char current_char = '0';
		uint32_t current_word = 0;
		file_location = 0;
		// needed for words_f as it saves a letter as 5 bits instead of 8.
		uint64_t file_five_bit_location = 0;
		BitByte current_byte;
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
		size_t additional_file_location = 0;
		size_t additional_id = 1;
		for (const words_reversed& reversed : words_reversed_l) {
			// it just needs a reversed block and no additional.
			ReversedBlock current_ReversedBlock{};
			if (reversed.reversed.size() <= 4) {
				for (int i = 0; i < reversed.reversed.size(); ++i) {
					current_ReversedBlock.ids[i] = reversed.reversed[i] + 1; //paths are indexed from 1 because 0 is reserved for empty values.	
				}
			} else {
				int additional_i = 0;
				int reversed_i = 0;
				AdditionalBlock additional{};
				for(const uint32_t& path_id : reversed.reversed) {
					if(reversed_i < 4) {
						current_ReversedBlock.ids[reversed_i] = path_id + 1;
						++reversed_i;
						continue;
					}
					if(reversed_i == 4) {
						current_ReversedBlock.ids[4] = additional_id;
						++reversed_i;
					}
					if(additional_i == 24) {// the next additional id field.
						additional.ids[24] = additional_id + 1;
						additional_i = 0;
						for (const char& byte : additional.bytes) {
							mmap_additional[additional_file_location] = byte;
							++additional_file_location;
						}
						++additional_id;
						additional = {};
					}
					additional.ids[additional_i] = path_id + 1;
					++additional_i;
				}
				if (additional_i != 0) {
					for (const char& byte : additional.bytes) {
						mmap_additional[additional_file_location] = byte;
						++additional_file_location;
					}
					++additional_id;
				}
			}

			// write reversed block
			for (int i = 0; i < 10; ++i) {
				mmap_reversed[file_location] = current_ReversedBlock.bytes[i];
				++file_location;
			}
		}
		reversed_size = file_location;
		log::write(2, "indexer: add: reversed and additonal written");
		
		unmap();
                resize(index_path / "paths_count.index", paths_count_size);
                resize(index_path / "paths.index", paths_size);
		resize(index_path / "words.index", words_size);
		resize(index_path / "words_f.index", words_f_size);
		// no resizing of reversed and additional as we can calculate its needed space at the beginning.
		map();
		first_time = false;
		
		if (ec) {
			log::write(3, "Index: add: error");
			return 1;
		}
	} else {
		log::write(2, "indexer: add: adding to existing index.");
		std::vector<Transaction> transactions;		
		size_t paths_size = paths.size();
		uint16_t paths_mapping[paths_size] = {};

		//convert local paths to a map with path -> id
		std::unordered_map<std::string, uint16_t> paths_search;
		size_t path_insertion_count = 0;
		for (const std::string& s : paths) {
			paths_search[s] = path_insertion_count;
			++path_insertion_count;
		}
		paths.clear();

		//go through index on disk and map disk Id to local Id.
		size_t on_disk_count = 0;
		size_t on_disk_id = 0;
		uint16_t next_path_end = 0; // if 0 the next 2 values are the header.

		while (on_disk_count < paths_size) {
			if (on_disk_count + 1 < paths_size) { // as we read 1 byte ahead to prevent accessing invalid data. The index format would allow it but it could be corrupted and not detected.
				PathOffset path_offset;
				path_offset.bytes[0] = mmap_paths[on_disk_count];
				++on_disk_count;
				path_offset.bytes[1] = mmap_paths[on_disk_count];
				next_path_end = path_offset.offset;
				++on_disk_count;
			} else { 
				// Abort. A corrupted index would mess things up. If the corruption could not get detectet or fixed before here it is most likely broken.
				log::error("Index: Combine: invalid path content length indicator. Aborting. The Index could be corrupted.");
			}

			if (on_disk_count + next_path_end < paths_size) { // check if we can read all of it.
				std::string path_to_compare(&mmap_paths[on_disk_count], next_path_end);
				if (paths_search.find(path_to_compare) != paths_search.end()) {
					paths_mapping[paths_search[path_to_compare]] = on_disk_id;
					paths_search.erase(path_to_compare);
				}
			} else { 
				log::error("Index: Combine: invalid path content length. Aborting. The Index could be corrupted.");
			}
			++on_disk_id;
		}

		// go through all remaining paths_search elements, add a transaction and add a new id to paths_mapping.
		for (const auto& [key, value] : paths_search) {
			paths_mapping[value] = on_disk_id;
			Transaction to_add_path_transaction;
			to_add_path_transaction.content = key;
			to_add_path_transaction.header.status = 0;
			to_add_path_transaction.header.index_type = 0;
			to_add_path_transaction.header.location = paths_size - 1;
			to_add_path_transaction.header.backup_id = 0;
			to_add_path_transaction.header.operation_type = 1;
			to_add_path_transaction.header.content_length = key.length();
			++on_disk_id;
			transactions.push_back(to_add_path_transaction);
		}
		paths_search.clear();
	}
	return 0;
}

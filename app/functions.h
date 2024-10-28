// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <filesystem>
#include <array>
#include <unordered_set>
#include <vector>
#include "lib/mio.hpp"
#include "lib/english_stem.h"
#include <unordered_map>
#include <future>

// local_index.cpp

struct words_reversed {
	std::wstring word;
	std::vector<uint32_t> reversed;

	bool operator<(const words_reversed& other) const {
     		return word < other.word;
    	}
};

class local_index {
	private:
		std::vector<std::string> paths;
		size_t paths_size;
		std::vector<words_reversed> words_and_reversed;
		size_t words_size;
		size_t reversed_size;
		std::vector<uint32_t> path_word_count;
		size_t path_word_count_size;
	public:
		local_index();
		int size();
		void clear();
		uint32_t add_path(const std::string& path_to_insert);
		void add_words(std::unordered_set<std::wstring>& words_to_insert, uint32_t path_id);
		void sort();
		void add_to_disk();
		void combine(local_index& to_combine_index);

	friend void combine(local_index& to_combine_index);
};

struct threads_jobs {
        std::future<local_index> future;
        uint32_t queue_id;
};

// indexer.cpp
class indexer {
	private:
		static stemming::english_stem<> StemEnglish;
		static bool config_loaded;
		static bool scan_dot_paths;
		static std::filesystem::path path_to_scan;
		static int threads_to_use;
		static size_t local_index_memory;
	private:
		static bool extension_allowed(const std::filesystem::path& path);
		static std::unordered_set<std::wstring> get_words_text(const std::filesystem::path& path);
		static std::unordered_set<std::wstring> get_words(const std::filesystem::path& path);
		static local_index thread_task(const std::vector<std::filesystem::path> paths_to_index);
		static bool queue_has_place(const std::vector<size_t>& batch_queue_added_size, const size_t& filesize, const size_t& max_filesize, const uint32_t& current_batch_add_start);
	public:
		static void save_config(const bool config_scan_dot_paths, const std::filesystem::path& config_path_to_scan, const int config_threads_to_use, const size_t& config_local_index_memory);
		static int start_from();
};

// helper.cpp
class helper {
	private:
		static const std::array<char, 256> conversion_table;
		static const std::unordered_map<char, char> special_chars;
	public:
		static int file_size(const std::filesystem::path& file_path);
		static std::string file_extension(const std::filesystem::path& path);
		static void convert_char(char& c);
};

// log.cpp
class log {
        private:
                static int min_log_level;
                static constexpr std::array<std::string_view, 4> log_level_text = {"[DEBUG]: ", "[INFO]: ", "[WARN]: ", "[CRITICAL]: "};
        public:
                static void save_config(const int config_min_log_level);
                static void write(const int log_level, const std::string& log_message);
		static void error(const std::string& error_message);
};

// index.cpp
class index {
        private:
                static bool is_config_loaded;
                static bool is_mapped;
                static bool first_time;
		static int buffer_size;
                static std::filesystem::path index_path;
                static int paths_size;
                static int paths_size_buffer;
		static int paths_count_size;
		static int paths_count_size_buffer;
                static int words_size;
                static int words_size_buffer;
                static int words_f_size;
                static int words_f_size_buffer;
                static int reversed_size;
                static int reversed_size_buffer;
                static int additional_size;
                static int additional_size_buffer;
                static mio::mmap_sink mmap_paths;
		static mio::mmap_sink mmap_paths_count;
                static mio::mmap_sink mmap_words;
                static mio::mmap_sink mmap_words_f;
                static mio::mmap_sink mmap_reversed;
                static mio::mmap_sink mmap_additional;
        private:
		static void check_files();
                static int get_actual_size(const mio::mmap_sink& mmap);
                static int map();
                static int unmap();
                static void resize(const std::filesystem::path& path_to_resize, const int size);
        public:
		static void save_config(const std::filesystem::path& config_index_path, const int config_buffer_size);
                static bool is_index_mapped();
		static int initialize();
                static int uninitialize();	
		static int add(std::vector<std::string>& paths, const size_t& paths_size_l, std::vector<uint32_t>& paths_count, const size_t& path_count_size_l, std::vector<words_reversed>& words_reversed_l, const size_t& words_size_l, const size_t& reversed_size_l);
};


#endif

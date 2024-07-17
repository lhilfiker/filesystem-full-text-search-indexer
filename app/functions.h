// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string>
#include <filesystem>
#include <array>
#include <unordered_set>
#include <vector>
#include "lib/mio.hpp"

// local_index.cpp
class local_index {
	private:
		std::vector<std::string> paths;
		std::vector<std::wstring> words;
		std::vector<std::vector<uint32_t>> reversed;
	public:
		uint32_t add_path(const std::string& path_to_insert);
		void add_words(std::unordered_set<std::wstring>& words_to_insert, uint32_t path_id);

};

// indexer.cpp
class indexer {
	private:
		static bool scan_dot_paths;
	public:
		static void save_config(bool config_scan_dot_paths);
		static int start_from(const std::filesystem::file_time_type& from_time);
};

// helper.cpp
class helper {
	private:
	public:
		static int file_size(const std::filesystem::path& file_path);
};

// log.cpp
class log {
        private:
                static int min_log_level;
                static constexpr std::array<std::string_view, 4> log_level_text = {"[DEBUG]: ", "[INFO]: ", "[WARN]: ", "[CRITICAL]: "};
        public:
                static void save_config(const int config_min_log_level);
                static void write(const int log_level, const std::string& log_message);
};

// index.cpp
class index {
        private:
                static bool is_config_loaded;
                static bool is_mapped;
                static int buffer_size;
                static std::filesystem::path index_path;
                static int paths_size;
                static int paths_size_buffer;
                static int words_size;
                static int words_size_buffer;
                static int words_f_size;
                static int words_f_size_buffer;
                static int reversed_size;
                static int reversed_size_buffer;
                static int additional_size;
                static int additional_size_buffer;
                static mio::mmap_sink mmap_paths;
                static mio::mmap_sink mmap_words;
                static mio::mmap_sink mmap_words_f;
                static mio::mmap_sink mmap_reversed;
                static mio::mmap_sink mmap_additional;
                static int check_files();
                static int get_actual_size(const mio::mmap_sink& mmap);
                static int map();
                static int unmap();
                static int resize(const std::filesystem::path& path_to_resize, const int size);
        public:
                static void save_config(const std::filesystem::path& config_index_path, int config_buffer_size);
                static bool is_mapped();
		static int initialize();
                static int uninitialize();
};


#endif

#include "functions.h"
#include <filesystem>
#include <string>

int helper::file_size(const std::filesystem::path& file_path) {
	int size = -1;
	std::error_code ec;
	size = std::filesystem::file_size(file_path, ec);
	return size;
}

void helper::convert_char(char& c) {
	// 'A' -> 'a', 'a' -> 'a', '1' -> ''	
}

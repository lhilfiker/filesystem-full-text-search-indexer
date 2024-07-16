#include "functions.h"
#include <filesystem>
#include <string>


int helper::file_size(std::filesystem::path file_path) {
	int size = -1;
	std::error_code ec;
	size = std::filesystem::file_size(file_path, ec);
	return size;
}


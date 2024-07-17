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
	if (c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' || c == 'g' || c == 'h' || c == 'i' || c == 'j' || c == 'k' || c == 'l' || c == 'm' || c == 'n' || c == 'o' || c == 'p' || c == 'q' || c == 'r' || c == 's' || c == 't' || c == 'u' || c == 'v' || c == 'w' || c == 'x' || c == 'y' || c == 'z') {}
	else if (c == 'A') c = 'a';
	else if (c == 'B') c = 'b';
	else if (c == 'C') c = 'c';
	else if (c == 'D') c = 'd';
	else if (c == 'E') c = 'e';
	else if (c == 'F') c = 'f';
	else if (c == 'G') c = 'g';
	else if (c == 'H') c = 'h';
	else if (c == 'I') c = 'i';
	else if (c == 'J') c = 'j';
	else if (c == 'K') c = 'k';
	else if (c == 'L') c = 'l';
	else if (c == 'M') c = 'm';
	else if (c == 'N') c = 'n';
	else if (c == 'O') c = 'o';
	else if (c == 'P') c = 'p';
	else if (c == 'Q') c = 'q';
	else if (c == 'R') c = 'r';
	else if (c == 'S') c = 's';
	else if (c == 'T') c = 't';
	else if (c == 'U') c = 'u';
	else if (c == 'V') c = 'v';
	else if (c == 'W') c = 'w';
	else if (c == 'X') c = 'x';
	else if (c == 'Y') c = 'y';
	else if (c == 'Z') c = 'z';
	else c = ' ';
	return;
}

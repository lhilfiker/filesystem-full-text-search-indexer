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
	if (c == 'e' || c == 't' || c == 'a' || c == 'o' || c == 'i' || c == 'n' || c == 's' || c == 'h' || c == 'r' || c == 'd' || c == 'l' || c == 'u' || c == 'c' || c == 'm' || c == 'w' || c == 'f' || c == 'g' || c == 'y' || c == 'p' || c == 'b' || c == 'v' || c == 'k' || c == 'j' || c == 'x' || c == 'q' || c == 'z') {
    		// lowercase letter, do nothing
	}
	else if (c == 'E') c = 'e';
	else if (c == 'T') c = 't';
	else if (c == 'A') c = 'a';
	else if (c == 'O') c = 'o';
	else if (c == 'I') c = 'i';
	else if (c == 'N') c = 'n';
	else if (c == 'S') c = 's';
	else if (c == 'H') c = 'h';
	else if (c == 'R') c = 'r';
	else if (c == 'D') c = 'd';
	else if (c == 'L') c = 'l';
	else if (c == 'U') c = 'u';
	else if (c == 'C') c = 'c';
	else if (c == 'M') c = 'm';
	else if (c == 'W') c = 'w';
	else if (c == 'F') c = 'f';
	else if (c == 'G') c = 'g';
	else if (c == 'Y') c = 'y';
	else if (c == 'P') c = 'p';
	else if (c == 'B') c = 'b';
	else if (c == 'V') c = 'v';
	else if (c == 'K') c = 'k';
	else if (c == 'J') c = 'j';
	else if (c == 'X') c = 'x';
	else if (c == 'Q') c = 'q';
	else if (c == 'Z') c = 'z';
	// Special characters
	else if (c == 'ä' || c == 'Ä' || c == 'å' || c == 'Å' || c == 'à' || c == 'À' || c == 'á' || c == 'Á' || c == 'â' || c == 'Â' || c == 'ã' || c == 'Ã') c = 'a';
	else if (c == 'ö' || c == 'Ö' || c == 'ò' || c == 'Ò' || c == 'ó' || c == 'Ó' || c == 'ô' || c == 'Ô' || c == 'õ' || c == 'Õ') c = 'o';
	else if (c == 'é' || c == 'É' || c == 'è' || c == 'È' || c == 'ê' || c == 'Ê' || c == 'ë' || c == 'Ë') c = 'e';
	else if (c == 'ü' || c == 'Ü' || c == 'ù' || c == 'Ù' || c == 'ú' || c == 'Ú' || c == 'û' || c == 'Û') c = 'u';
	else if (c == 'ì' || c == 'Ì' || c == 'í' || c == 'Í' || c == 'î' || c == 'Î' || c == 'ï' || c == 'Ï') c = 'i';
	else if (c == 'ñ' || c == 'Ñ') c = 'n';
	else if (c == 'ý' || c == 'Ý' || c == 'ÿ' || c == 'Ÿ') c = 'y';
	else if (c == 'ç' || c == 'Ç') c = 'c';
	else if (c == 'ß') c = 'ss';
	else c = '!';
	return;
}

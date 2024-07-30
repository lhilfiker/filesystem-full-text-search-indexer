#include "functions.h"
#include <filesystem>
#include <string>
#include <unordered_map>

int helper::file_size(const std::filesystem::path& file_path) {
	int size = -1;
	std::error_code ec;
	size = std::filesystem::file_size(file_path, ec);
	return size;
}

void helper::convert_char(char& c) {
	if (c >= 0 && c < 256) {
            c = conversion_table[static_cast<unsigned char>(c)];
        } else {
            auto it = special_chars.find(c);
            if (it != special_chars.end()) {
                c = it->second;
            } else {
                c = '!';
            }
        }
	return;
}

const std::array<char, 256> helper::conversion_table = []() {
    std::array<char, 256> table;
    for (int i = 0; i < 256; ++i) {
        char c = static_cast<char>(i);
        if (c >= 'a' && c <= 'z') {
            table[i] = c;
        } else if (c >= 'A' && c <= 'Z') {
            table[i] = c - 'A' + 'a';
        } else {
            table[i] = '!';
        }
    }
    return table;
}();

// Initialize the special characters map
const std::unordered_map<char, char> helper::special_chars = {
    {'ä', 'a'}, {'Ä', 'a'}, {'å', 'a'}, {'Å', 'a'}, {'à', 'a'}, {'À', 'a'},
    {'á', 'a'}, {'Á', 'a'}, {'â', 'a'}, {'Â', 'a'}, {'ã', 'a'}, {'Ã', 'a'},
    {'ö', 'o'}, {'Ö', 'o'}, {'ò', 'o'}, {'Ò', 'o'}, {'ó', 'o'}, {'Ó', 'o'},
    {'ô', 'o'}, {'Ô', 'o'}, {'õ', 'o'}, {'Õ', 'o'},
    {'é', 'e'}, {'É', 'e'}, {'è', 'e'}, {'È', 'e'}, {'ê', 'e'}, {'Ê', 'e'},
    {'ë', 'e'}, {'Ë', 'e'},
    {'ü', 'u'}, {'Ü', 'u'}, {'ù', 'u'}, {'Ù', 'u'}, {'ú', 'u'}, {'Ú', 'u'},
    {'û', 'u'}, {'Û', 'u'},
    {'ì', 'i'}, {'Ì', 'i'}, {'í', 'i'}, {'Í', 'i'}, {'î', 'i'}, {'Î', 'i'},
    {'ï', 'i'}, {'Ï', 'i'},
    {'ñ', 'n'}, {'Ñ', 'n'},
    {'ý', 'y'}, {'Ý', 'y'}, {'ÿ', 'y'}, {'Ÿ', 'y'},
    {'ç', 'c'}, {'Ç', 'c'},
    {'ß', 's'}
};

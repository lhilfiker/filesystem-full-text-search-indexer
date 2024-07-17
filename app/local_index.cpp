#include "functions.h"
#include <vector>
#include <string>
#include <unordered_set>

std::vector<std::string> local_index::paths;
std::vector<std::wstring> local_index::words;
std::vector<std::vector<uint32_t>> local_index::reversed;

uint32_t local_index::add_path(const std::string& path_to_insert) {
	uint32_t id = 0;
	for (const std::string& path : paths) {
		if (path == path_to_insert) {
			return id;
		}
		++id;
	}
	paths.push_back(path_to_insert);
	return id;
}

void local_index::add_words(std::unordered_set<std::wstring>& words_to_insert, uint32_t path_id) {
	uint32_t id = 0;
	for (const std::wstring& word : words) {
		if (words_to_insert.erase(word) == 1) {
			reversed[id].push_back(path_id);
		}
		++id;
	}
	for (const std::wstring& word : words_to_insert) {
		words.push_back(word);
		reversed.push_back(std::vector<uint32_t>{path_id});
	}
	return;
}

#include "functions.h"
#include <vector>
#include <string>
#include <unordered_set>

std::vector<std::string> local_index::paths;
std::vector<words_reversed> local_index::words_and_reversed;

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
	for (words_reversed& word_r : words_and_reversed) {
		if (words_to_insert.erase(word_r.word) == 1) {
			word_r.reversed.push_back(path_id);
		}
		++id;
	}
	for (const std::wstring& word : words_to_insert) {
		words_and_reversed.push_back({word, {path_id}});
	}
	return;
}

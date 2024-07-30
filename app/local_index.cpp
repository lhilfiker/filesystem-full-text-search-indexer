#include "functions.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>


local_index::local_index() {
	paths_size = 0;
	words_size = 0;
	reversed_size = 0;
	path_word_count_size = 0;
	words_and_reversed.reserve(1000000); // This should later be the set memory usage. For now a temp value
}

int local_index::size() {
	return paths_size + words_and_reversed_size + path_word_count_size;
}

void local_index::clear() {
	paths.clear();
	words_and_reversed.clear();
	path_word_count.clear();
	paths_size = 0;
        words_size = 0;
	reversed_size = 0;
        path_word_count_size = 0;
	return;
}

uint32_t local_index::add_path(const std::string& path_to_insert) {
	uint32_t id = 0;
	for (const std::string& path : paths) {
		if (path == path_to_insert) {
			return id;
		}
		++id;
	}
	paths.push_back(path_to_insert);
	paths_size += path_to_insert.length();
	path_word_count.push_back(0);
	++path_word_count_size;
	return id;
}

void local_index::add_words(std::unordered_set<std::wstring>& words_to_insert, uint32_t path_id) {
	uint32_t word_count = 0;
	for (words_reversed& word_r : words_and_reversed) {
		if (words_to_insert.erase(word_r.word) == 1) {
			word_r.reversed.push_back(path_id);
			reversed_size += sizeof(path_id);
			++word_count;
		}
	}
	for (const std::wstring& word : words_to_insert) {
		words_and_reversed.push_back({word, {path_id}});
		words_size += word.length();
		reversed_size += sizeof(path_id);
		++word_count;
	}
	path_word_count_size += sizeof(word_count) - sizeof(path_word_count[path_id]);
	path_word_count[path_id] = word_count;
	return;
}

void local_index::sort() {
	std::sort(words_and_reversed.begin(), words_and_reversed.end());
	return;
}

void local_index::add_to_disk() {
	index::add(paths, paths_size, path_word_count, path_word_count_size, words_and_reversed, words_size, reversed_size);
	clear();
	return;
}

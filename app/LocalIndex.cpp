#include "functions.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>


LocalIndex::LocalIndex() {
	paths_size = 0;
	words_size = 0;
	reversed_size = 0;
	path_word_count_size = 0;
}

int LocalIndex::size() {
	return paths_size + words_size + paths_size + path_word_count_size + reversed_size;
}

void LocalIndex::clear() {
	paths.clear();
	words_and_reversed.clear();
	path_word_count.clear();
	paths_size = 0;
        words_size = 0;
	reversed_size = 0;
        path_word_count_size = 0;
	return;
}

uint32_t LocalIndex::add_path(const std::string& path_to_insert) {
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

void LocalIndex::add_words(std::unordered_set<std::wstring>& words_to_insert, uint32_t path_id) {
	uint32_t word_count = 0;
	for (words_reversed& word_r : words_and_reversed) {
		if (words_to_insert.erase(word_r.word) == 1) {
			word_r.reversed.insert(path_id);
			reversed_size += sizeof(path_id);
			++word_count;
		}
	}
	for (const std::wstring& word : words_to_insert) {
		if(word.length() == 0) continue;
		words_and_reversed.push_back({word, {path_id}});
		words_size += word.length();
		reversed_size += sizeof(path_id);
		++word_count;
	}
	path_word_count_size += sizeof(word_count) - sizeof(path_word_count[path_id]);
	path_word_count[path_id] = word_count;
	return;
}

void LocalIndex::sort() {
	std::sort(words_and_reversed.begin(), words_and_reversed.end());
	return;
}

void LocalIndex::add_to_disk() {
	if (paths_size == 0 || path_word_count_size == 0 || reversed_size == 0 || words_size == 0) return; // do not try to add if we don't have anythign to add.
	index_combine_data index_to_add{paths, paths_size, path_word_count, path_word_count_size, words_and_reversed, words_size, reversed_size };
	Index::add(index_to_add);
	clear();
	return;
}
 
void LocalIndex::combine(LocalIndex& to_combine_index) {
	log::write(1, "LocalIndex: combine: start");
	// if empty just add directly
	if (paths_size == 0 && words_size == 0 && reversed_size == 0 && path_word_count_size == 0) {
		paths = to_combine_index.paths;
		paths_size = to_combine_index.paths_size;
		words_and_reversed = to_combine_index.words_and_reversed;
		words_size = to_combine_index.words_size;
		reversed_size = to_combine_index.reversed_size;
		path_word_count = to_combine_index.path_word_count;
		path_word_count_size = to_combine_index.path_word_count_size;
		log::write(1, "copied as empty");
		return;
	}

	std::vector<uint32_t> paths_id;
	size_t paths_last = paths.size();
	int i = 0;

	for (const std::string& path_to_insert : to_combine_index.paths) {
		if (auto loc = std::find(paths.begin(), paths.end(), path_to_insert); loc != std::end(paths)) {
			size_t it = std::distance(paths.begin(), loc);
			paths_id.push_back(static_cast<uint32_t>(it));
			path_word_count[it] = to_combine_index.path_word_count[i];
		}
		else {
			paths.push_back(path_to_insert);
			paths_size += path_to_insert.length();
			paths_id.push_back(paths_last);
			path_word_count.push_back(to_combine_index.path_word_count[i]);
			path_word_count_size += sizeof(to_combine_index.path_word_count[i]);
			++paths_last;
		}
		++i;
	}
	
	// sort to compare them by the alphabet.
	sort();
	to_combine_index.sort();
	
	int words_reversed_count = words_and_reversed.size();
	int to_combine_count = to_combine_index.words_and_reversed.size();
	if (to_combine_index.words_and_reversed.empty()) return;
	int local_counter = 0;
	int to_combine_counter = 0;
	if (to_combine_index.words_and_reversed.size() != 0) {
		while(local_counter < words_reversed_count) {
			// if found add converted path ids
			if (words_and_reversed[local_counter].word == to_combine_index.words_and_reversed[to_combine_counter].word) {
				for (const uint32_t& remote_id : to_combine_index.words_and_reversed[to_combine_counter].reversed) {
						words_and_reversed[local_counter].reversed.insert(paths_id[remote_id]);
						reversed_size += sizeof(paths_id[remote_id]);
				}	
				++to_combine_counter;
				if (to_combine_counter >= to_combine_count) {
					to_combine_index.clear();
					return; // finished if no more to_combine_elements.
				}
			}
			// if it wasn't found and we went passed it, add a new word.
			if (words_and_reversed[local_counter].word > to_combine_index.words_and_reversed[to_combine_counter].word) {	
				std::unordered_set<uint32_t> to_add_ids;
				for (const uint32_t& remote_id : to_combine_index.words_and_reversed[to_combine_counter].reversed) {
						to_add_ids.insert(paths_id[remote_id]);
        	                        	reversed_size += sizeof(paths_id[remote_id]);
 
				}
				words_and_reversed.push_back({to_combine_index.words_and_reversed[to_combine_counter].word,to_add_ids});
				words_size += to_combine_index.words_and_reversed[to_combine_counter].word.length();
				// we insert it at the end. We will not update local counter or max size because we dont need to reprocess already proccessed ones.
				++to_combine_counter;
				if (to_combine_counter >= to_combine_count) {
					to_combine_index.clear();
					return; // finished if no more to_combine_elements.
				}
			}
			++local_counter;
		}
	
		// add missing
		while(to_combine_counter >= to_combine_count) {
			std::unordered_set<uint32_t> to_add_ids;
        	        for (const uint32_t& remote_id : to_combine_index.words_and_reversed[to_combine_counter].reversed) {
					to_add_ids.insert(paths_id[remote_id]);
        	                	reversed_size += sizeof(paths_id[remote_id]);
			}
        	        words_and_reversed.push_back({to_combine_index.words_and_reversed[to_combine_counter].word,to_add_ids});
        	        words_size += to_combine_index.words_and_reversed[to_combine_counter].word.length();
        		++to_combine_counter;
		}
	}
	to_combine_index.clear();
	return;
}

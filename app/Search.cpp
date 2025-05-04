#include "functions.h"
#include "iostream"
#include "string"
#include <cstdint>
#include <string>
#include <unordered_map>

// Default values
bool Search::config_exact_match = true;
uint8_t Search::config_min_char_for_match = 4;

// To overwrite them
void Search::save_config(bool exact_match, uint8_t min_char_for_match) {
  config_exact_match = exact_match;
  config_min_char_for_match = min_char_for_match;
}

void Search::search() {
  // Ask the user to input text.
  std::string input = "";
  std::cout << "Search\n\nEnter Search Query(Search by pressing ENTER):\n";
  std::getline(std::cin, input);

  // split it into a vector by whitespaces, dots, etc... convert each char
  std::vector<std::string> search_words;
  std::string current_word = "";
  current_word.reserve(100);
  for (char c : input) {
    helper::convert_char(c);
    if (c == '!') {
      if (current_word.length() > 1 &&
          current_word.length() < 100) { // allow bigger and smaller words.
        // stem word
        search_words.push_back(current_word);
      }
      current_word.clear();
    } else {
      current_word.push_back(c);
    }
  }
  // Add last word
  if (current_word.length() > 1 &&
      current_word.length() < 100) { // allow bigger and smaller words.
    // stem word
    search_words.push_back(current_word);
  }

  // get path ids and count of the search query
  std::vector<search_path_ids_return> path_ids_count = Index::search_word_list(
      search_words, config_exact_match, config_min_char_for_match);

  // get the paths string from path ids.
  std::unordered_map<PATH_ID_TYPE, std::string> path_string =
      Index::id_to_path_string(path_ids_count);

  // sort path id and count to display from most to least
  std::sort(path_ids_count.begin(), path_ids_count.end(),
            [](const search_path_ids_return &a,
               const search_path_ids_return &b) { return a.count > b.count; });

  // output the results
  std::cout << "\n\nSearch Results:\n";
  for (int i = 0; i < path_ids_count.size(); ++i) {
    std::cout << "\n " << std::to_string(i) << ". "
              << path_string[path_ids_count[i].path_id] << " with "
              << std::to_string(path_ids_count[i].count) << " matches.\n";
  }
}
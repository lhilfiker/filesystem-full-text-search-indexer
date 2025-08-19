#include "search.h"
#include "../Helper/helper.h"
#include "../Index/index.h"
#include "../Index/index_types.h"
#include "iostream"
#include "string"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// Default values
bool Search::config_exact_match =
    false; // If true, this will make each word regardless of "" in the search
           // query be an exact match.
uint8_t Search::config_min_char_for_match = 4;

// To overwrite them
void Search::save_config(bool exact_match, uint8_t min_char_for_match) {
  config_exact_match = exact_match;
  config_min_char_for_match = min_char_for_match;
}

std::vector<search_path_ids_return>
Search::query_search(const std::string &query) {

  std::vector<search_path_ids_return> result;
  // We get a full search string query which we will first run queries on the
  // index on all words and then process it according to the query.
  if (query.length() == 0) {
    return result;
  }
  if (query[0] != '(' || query[query.size() - 1] != ')') {
    return result; // it needs to begin and end with it.
  }

  std::vector<std::pair<std::string, bool>> search_words;

  // extract all words from the query only respecting exact match and add them
  // to search_words.
  std::string current_word = "";
  bool exact_match_query = false;
  for (char c : query) {
    if (c == '"') {
      if (exact_match_query) {
        if (current_word.length() > 2 && current_word.length() < 254) {
          search_words.emplace_back(current_word, true);
          current_word.clear();
          exact_match_query = false;
        }
      } else {
        exact_match_query = true;
      }
    }
    if (Helper::convert_char(c); c == '!') {
      if (current_word.length() > 3 && current_word.length() < 254) {
        if (current_word == "AND" || current_word == "OR" ||
            current_word == "NOT") {
          current_word.clear();
          continue;
        }
        if (config_exact_match) {
          search_words.emplace_back(current_word, true);

        } else {
          search_words.emplace_back(current_word, false);
        }
        current_word.clear();
      }
    } else {
      current_word += c;
    }
  }
  if (current_word.length() > 3 && current_word.length() < 254) {
    if (current_word == "AND" || current_word == "OR" ||
        current_word == "NOT") {
    } else if (config_exact_match) {
      search_words.emplace_back(current_word, true);
    } else {
      search_words.emplace_back(current_word, false);
    }
  }

  // run search on index, we need to sort it first so we can connect each words
  // to it's path ids. we also remove all duplicates (only if also exact match
  // is the same)
  std::sort(search_words.begin(), search_words.end());
  auto last = std::unique(search_words.begin(), search_words.end());
  search_words.erase(last, search_words.end());

  std::vector<search_path_ids_count_return> word_search_results =
      Index::search_word_list(search_words, config_min_char_for_match);

  std::vector<std::unordered_map<PATH_ID_TYPE, uint32_t>>
      query_sub_result_table;

  std::vector<std::pair<uint8_t, uint32_t>> query_processing_table;
  // first is the type:
  // 0 = (
  // 1 = query_sub_block // this points to query_sub_result_table entry
  // 2 = word id // this points to word_search_result entry for the
  // corresponding word.
  // 3 = operator (second is 0 = OR, 1 = AND, 2 = NOT)
  // second is either position (for 0) or the iterator

  current_word.clear();
  exact_match_query = false;
  for (int i = 0; i < query.length(); ++i) {
    if (query[i] == '(') {
      query_processing_table.emplace_back(0, i);
      continue;
    }
    if (query[i] == ')') {
      // we need to process all entries in the processing table unril we reach
      // the earliest '(', then we process all and remove them and just add a 1
      // for query sub result.
      if (query_processing_table.size() == 0)
        return result;
      uint32_t opening_sub_query = 0;
      for (int j = query_processing_table.size() - 1; j >= 0; --j) {
        if (query_processing_table[j].first == 0) {
          opening_sub_query = j;
          break;
        }
      }

      std::unordered_map<PATH_ID_TYPE, uint32_t> temp_comparision;
      uint32_t sub_query_counter = 0;
      int current_operator = 0;
      for (int j = opening_sub_query; j < query_processing_table.size(); j++) {
        if (sub_query_counter == 0 ||
            query_processing_table[j] ==
                std::make_pair<uint8_t, uint32_t>(
                    3, 0)) { // first ones or OR operator, we will just add them
                             // to the temp comparision table
          switch (query_processing_table[j].first) {
          case 1:

            temp_comparision.insert(
                query_sub_result_table[query_processing_table[j].second]
                    .begin(),
                query_sub_result_table[query_processing_table[j].second].end());
            // copy it because it's first in here.
            break;
          case 2:
            for (int k = 0;
                 k < word_search_results[query_processing_table[j].second]
                         .path_id.size();
                 ++k) {
              temp_comparision.insert(
                  {word_search_results[query_processing_table[j].second]
                       .path_id[k],
                   word_search_results[query_processing_table[j].second]
                       .count[k]});
              // insert all because it's first.
            }
            break;
          case 3:
            continue;
            // first item is a operator which is invalid so we just ignore it.
          default:
            break;
          }
          current_operator = 0;
        }
        if (query_processing_table[j].first == 3) { // sub queries
          current_operator = query_processing_table[j].second;
        }
        if (query_processing_table[j].first == 1) {
          if (current_operator == 0) { // OR
            for (auto &element :
                 query_sub_result_table[query_processing_table[j].second]) {
              temp_comparision[element.first] += element.second;
            }
          } else if (current_operator == 1) { // AND
            std::unordered_map<PATH_ID_TYPE, uint32_t> intersection;
            for (const auto &element : temp_comparision) {
              if (query_sub_result_table[query_processing_table[j].second]
                      .count(element.first)) {
                intersection[element.first] +=
                    element.second +
                    query_sub_result_table[query_processing_table[j].second]
                                          [element.first];
              }
            }
            temp_comparision = intersection;
          } else if (current_operator == 2) { // NOT
            for (const auto &element :
                 query_sub_result_table[query_processing_table[j].second]) {
              temp_comparision.erase(element.first);
            }
          }
          current_operator = 0;
        }
        if (query_processing_table[j].first == 2) { // words
          if (current_operator == 0) {              // OR
            for (int k = 0;
                 k < word_search_results[query_processing_table[j].second]
                         .path_id.size();
                 ++k) {
              temp_comparision
                  [word_search_results[query_processing_table[j].second]
                       .path_id[k]] +=
                  word_search_results[query_processing_table[j].second]
                      .count[k];
            }
          } else if (current_operator == 1) { // AND
            std::unordered_map<PATH_ID_TYPE, uint32_t> intersection;

            for (int k = 0;
                 k < word_search_results[query_processing_table[j].second]
                         .path_id.size();
                 ++k) {
              if (temp_comparision.count(
                      word_search_results[query_processing_table[j].second]
                          .path_id[k])) {
                intersection[word_search_results[query_processing_table[j]
                                                     .second]
                                 .path_id[k]] =
                    word_search_results[query_processing_table[j].second]
                        .count[k] +
                    temp_comparision
                        [word_search_results[query_processing_table[j].second]
                             .path_id[k]];
              }
            }
            temp_comparision = intersection;
          } else if (current_operator == 2) { // NOT
            for (const auto &element :
                 word_search_results[query_processing_table[j].second]
                     .path_id) {
              temp_comparision.erase(element);
            }
          }

          current_operator = 0;
        }

        ++sub_query_counter;
      }

      // delete and just add a sub result query.
      query_processing_table.erase(query_processing_table.begin() +
                                       opening_sub_query,
                                   query_processing_table.end());
      query_processing_table.push_back(
          std::make_pair(1, query_sub_result_table.size()));

      query_sub_result_table.push_back(temp_comparision);
      continue;
    }
    if (query[i] == '"') {
      if (exact_match_query) {
        if (current_word.length() > 2 && current_word.length() < 254) {
          auto it = std::find(search_words.begin(), search_words.end(),
                              std::make_pair(current_word, true));
          if (it != search_words.end()) {
            query_processing_table.emplace_back(2, it - search_words.begin());
          }
          current_word.clear();
        }
      } else {
        exact_match_query = true;
        continue;
      }
    }
    if (query[i] == ' ') {
      if (current_word == "AND" || current_word == "OR" ||
          current_word == "NOT") {
        query_processing_table.emplace_back(
            3, current_word == "AND" ? 1 : (current_word == "OR" ? 0 : 2));
      }
      if (current_word.length() > 3 && current_word.length() < 254) {
        auto it = std::find(search_words.begin(), search_words.end(),
                            std::make_pair(current_word, false));
        if (it != search_words.end()) {
          query_processing_table.emplace_back(2, it - search_words.begin());
        }
        current_word.clear();
      }
      continue;
    }
    char c = query[i];
    if (Helper::convert_char(c); c != '!') {
      current_word += c;
    }
  }
  // Now we have only one query sub block linked to the processing table. This
  // is the final result. Convert to return type
  for (const auto &element :
       query_sub_result_table[query_processing_table[0].second]) {
    result.push_back({element.first, element.second});
  }
  return result;
}

void Search::search() {
  // Ask the user to input text.
  std::string input = "";
  std::cout << "Search\n\nEnter Search Query(Search by pressing ENTER):\n";
  std::getline(std::cin, input);
  input = "(" + input + ")"; // add these because query-search expects it.

  std::vector<search_path_ids_return> search_result = query_search(input);
  if (search_result.size() == 0) {
    std::cout
        << "\n\nNo search results found or invalid search query.:\n"; // TODO:
                                                                      // better
                                                                      // error
                                                                      // message
  }

  std::unordered_map<PATH_ID_TYPE, std::string> path_string =
      Index::id_to_path_string(search_result); // get path strings

  // sort path id and count to display from most to least
  std::sort(search_result.begin(), search_result.end(),
            [](const search_path_ids_return &a,
               const search_path_ids_return &b) { return a.count > b.count; });

  // output the results
  std::cout << "\n\nSearch Results:\n";
  for (int i = 0; i < search_result.size(); ++i) {
    std::cout << "\n " << std::to_string(i) << ". "
              << path_string[search_result[i].path_id] << " with "
              << std::to_string(search_result[i].count) << " matches.\n";
  }
}

#include "functions.h"
#include "iostream"
#include "string"

void Search::search() {
  // Ask the user to input text.
  std::string input = " ";
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
}
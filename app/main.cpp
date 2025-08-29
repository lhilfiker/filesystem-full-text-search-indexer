#include "CliParser/cliparser.h"
#include "Config/config.h"
#include "Index/index.h"
#include "Indexer/indexer.h"
#include "Logging/logging.h"
#include "Search/search.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

void output_help() { std::cout << ""; }

int main(int argc, char *argv[]) {
  // parse args
  CliParser cli;
  cli.parse(argc, argv);

  auto config = cli.get_config_values();
  auto options = cli.get_options();
  auto search_query = cli.get_search_query();
  std::string config_file = getenv("HOME");
  config_file += "/.config/filesystem-full-text-search-indexer/config.txt";

  // see if config file location is set.
  for (const auto &pair : config) {
    if (pair.first == "config_file") {
      config_file = pair.second;
      break;
    }
  }

  Config::load(config, config_file);
  Index::initialize();

  bool options_used = false;
  if (!options.empty()) {
    int i = 0;
    while (options.size() > i) {
      if (options[i] == "i" || options[i] == "index") {
        // index
        options_used = true;
        indexer::start_from();
      }
      if (options[i] == "s" || options[i] == "search") {
        // interactive search
        options_used = true;
        while (true) {
          std::string input = "";
          std::cout << "Search\n\nEnter Search Query(Search by pressing "
                       "ENTER), exit by sending 'q' or 'quit':\n";
          std::getline(std::cin, input);
          if (input == "q" || input == "quit" || input == "exit") {
            break;
          }
          Search::search(input);
        }
      }
      if (options[i] == "h" || options[i] == "help") {
        // help
        options_used = true;
        output_help();
      }
      ++i;
    }
  }

  // TODO: search query
  if (search_query.empty() && !options_used) {
    output_help(); // if not query got sent or no valid option.
  } else {
    Search::search(search_query);
  }

  return 0;
}

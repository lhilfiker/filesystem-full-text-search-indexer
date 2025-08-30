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
#include <utility>

void output_help() {
  std::cout << "Filesystem Full Text Search Index\n";
  std::cout << "Usage:\n";
  std::cout << "  app [OPTIONS] [SEARCH_QUERY]\n";
  std::cout << "\nOptions:\n";
  std::cout << "  -i, --index                     Index files in the "
               "configured directory\n";
  std::cout
      << "  -s, --search                    Start interactive search mode\n";
               "of whether they have been updated or not.\n";
  std::cout << "  -c, --current                   Change config path to scan "
               "to current path for this session\n";
  std::cout << "  -h, --help                      Show this help message\n";
  std::cout << "\nConfig Options:\n";
  std::cout << "  --config_file=/full/path        Overwrites config file path "
               "from default\n";
  std::cout << "  --config_key=value              Overwrites config option\n";
  std::cout << "\nArguments:\n";
  std::cout << "  SEARCH_QUERY                    Search for the given query\n";
  std::cout << "\nExamples:\n";
  std::cout << "  app -i                          # Index files\n";
  std::cout << "  app -s                          # Start interactive search\n";
  std::cout << "  app myquery                     # Search for 'myquery'\n";
  std::cout << std::endl;
}

int main(int argc, char *argv[]) {
  // parse args
  CliParser cli;
  cli.parse(argc, argv);

  auto config = cli.get_config_values();
  auto options = cli.get_options();
  auto search_query = cli.get_search_query();

  if (std::find(options.begin(), options.end(), "help") != options.end() ||
      std::find(options.begin(), options.end(), "h") != options.end()) {
    // show help and exit.
    output_help();
    return 0;
  }

  const char *home_env = getenv("HOME");
  std::string config_file = "";
  if (home_env != nullptr) {
    config_file = std::string(home_env) +
                  "/.config/filesystem-full-text-search-indexer/config.txt";
  }

  // see if config file location is set.
  for (const auto &pair : config) {
    if (pair.first == "config_file") {
      config_file = pair.second;
      break;
    }
  }

  Config::load(config, config_file);
  Index::initialize();

  // options config overwrite loop
  if (!options.empty()) {
    int i = 0;
    while (options.size() > i) {
      if (options[i] == "a" || options[i] == "all") {
        // overwrite config to make it search all files, regardless of updated
        // status
        Config::load(
            std::vector<std::pair<std::string, std::string>>(
                {std::make_pair("config_updated_files_only", "false")}),
            "");
      }
      if (options[i] == "c" || options[i] == "current" ||
          options[i] == "current-dir") {
        // overwrite config which path to scan with current path.
        Config::load(
            std::vector<std::pair<std::string, std::string>>(
                {std::make_pair("config_updated_files_only", "false")}),
            "");
        Config::load(
            std::vector<std::pair<std::string, std::string>>({std::make_pair(
                "config_path_to_scan", std::filesystem::current_path())}),
            "");
      }
      ++i;
    }
  }

  // options action loop
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
          std::string input;
          std::cout << "Search\n\nEnter Search Query(Search by pressing "
                       "ENTER), exit by sending 'q' or 'quit':\n";
          std::getline(std::cin, input);
          if (input == "q" || input == "quit" || input == "exit") {
            break;
          }
          Search::search(input);
        }
      }
      ++i;
    }
  }

  // TODO: search query
  if (search_query == "" && !options_used) {
    output_help(); // if not query got sent or no valid option.
  } else {
    if (search_query != "") {
      Search::search(search_query);
    }
  }

  Index::uninitialize();

  return 0;
}

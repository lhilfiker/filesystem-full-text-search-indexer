#include "CliParser/cliparser.h"
#include "Config/config.h"
#include "Index/index.h"
#include "Indexer/indexer.h"
#include "Logging/logging.h"
#include "Search/search.h"
#include <algorithm>
#include <filesystem>
#include <string>

int main(int argc, char *argv[]) {
  // parse args
  CliParser cli;
  cli.parse(argc, argv);

  auto config = cli.get_config_values();
  auto options = cli.get_options();
  auto search_query = cli.get_search_query();
  std::string config_file = getenv("HOME");
  config_file += "./config/filesystem-full-text-search-index";

  // see if config file location is set.
  for (const auto &pair : config) {
    if (pair.first == "config_file") {
      config_file = pair.second;
      break;
    }
  }

  Config::load(config, config_file);
  Index::initialize();

  return 0;
}

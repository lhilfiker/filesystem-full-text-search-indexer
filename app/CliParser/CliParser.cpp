#include "cliparser.h"
#include "../Index/index.h"
#include "../Indexer/indexer.h"
#include "../Logging/logging.h"
#include "../Search/search.h"
#include <fstream>
#include <sstream>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

CliParser::CliParser() {
  options = {};
  config = {};
  search_query = "";
}

void CliParser::parse(int argc, char *argv[]) {
  int i = 1;

  while (i < argc) {
    std::string arg = argv[i];
    if (arg[0] != '-')
      break; // If it isn't an argument.

    if (arg.length() > 2 && arg[1] == '-') { // -- argument
      std::string full_arg = arg.substr(2);
      if (full_arg.find('=') != std::string::npos) {
        config.push_back(std::make_pair(
            full_arg.substr(0, full_arg.length() - full_arg.find('=')),
            full_arg.substr(full_arg.find('='))));
      } else {
        options.push_back(full_arg);
      }
    } else {
      options.push_back(arg.substr(1));
    }
    ++i;
  }
}

#include "cliparser.h"
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
        config.push_back(
            std::make_pair(full_arg.substr(0, full_arg.find('=')),
                           full_arg.substr(full_arg.find('=') + 1)));
      } else {
        options.push_back(full_arg);
      }
    } else {
      options.push_back(arg.substr(1));
    }
    ++i;
  }
  while (i < argc) {
    if (search_query.size() != 0) {
      search_query += " ";
    }
    search_query += argv[i];
    ++i;
  }
}

std::vector<std::pair<std::string, std::string>>
CliParser::get_config_values() {
  return config;
}
std::vector<std::string> CliParser::get_options() { return options; }
std::string CliParser::get_search_query() { return search_query; }

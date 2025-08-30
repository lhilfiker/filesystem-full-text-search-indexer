// cliparser.h
#ifndef CLIPARSER_H
#define CLIPARSER_H

#include "../lib/mio.hpp"
#include <array>
#include <filesystem>
#include <future>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// CliParser/CliParser.cpp
class CliParser {
private:
  std::vector<std::string> options;
  std::vector<std::pair<std::string, std::string>> config;
  std::string search_query;

public:
  CliParser();

  void parse(int argc, char *argv[]);
  std::vector<std::pair<std::string, std::string>> get_config_values();
  std::vector<std::string> get_options();
  std::string get_search_query();
};

#endif
#include "Index/index.h"
#include "Indexer/indexer.h"
#include "Logging/logging.h"
#include "Search/search.h"
#include <filesystem>
#include <string>

int test() {
  log::write(2, "main: starting application. loading config...");
  log::save_config(1);
  Index::save_config(
      "/home/lukas/.local/share/filesystem-full-text-search-indexer/");
  log::write(2, "config loaded.");
  log::write(2, "attempting to initialize index.");
  if (Index::initialize() == 1) {
    log::write(4, "error when initializing index");
    return 1;
  }
  log::write(2, "initialized successfully");
  log::write(2, "uninitializing now.");
  if (Index::uninitialize() == 1) {
    log::write(4, "uninitialize failed.");
    return 1;
  }
  indexer::save_config(false, "/home/lukas/Dokumente/incoming", 4, 1500000000);
  indexer::start_from();

  Search::search();

  log::write(2, "done, uninitializing...");
  if (Index::uninitialize() == 1) {
    log::write(4, "uninitialize failed.");
    return 1;
  }
  return 0;
}

int main() {
  if (test() == 1) {
    log::write(4, "main: test function returned error, exiting");
    return 1;
  }
  log::write(1, "done all");
  return 0;
}

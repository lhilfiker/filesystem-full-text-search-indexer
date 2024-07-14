#include "functions.h"
#include <string>
#include <filesystem>

int test() {
	log::write(2, "main: starting application. loading config...");
	log::save_config(1);
	index::save_config("~/.local/share/filesystem-full-text-search-indexer/", 500000);
	log::write(2, "config loaded.");
	log::write(2, "attempting to initialize index.");
	if (index::initialize() == 1) {
		log::write(4, "error when initializing index");
		return 1;
	}
	log::write(2, "initialized successfully");
	return 0;
}

int main() {
	if (test() == 1) {
		log::write(4, "main: test function returned error, exiting");
		return 1;
	}
	return 0;
}

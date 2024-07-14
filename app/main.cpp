#include "functions.h"
#include <string>
#include <filesystem>

int main() {
	if (test == 1) {
		return 1;
	}
	return 0;
}

int test() {
	log::write(2, "main: starting application. loading config...");
	log::save_config(1);
	index::save_config("~/.local/share/filesystem-full-text-search-indexer/", 500000);
	log::write(2, "config loaded.");
	return 0;
}

#include "functions.h"
#include "lib/mio.hpp"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

int Index::execute_transactions() {

    std::filesystem::remove(index_path / "transaction" / "transaction.list");
    return 0;
  }
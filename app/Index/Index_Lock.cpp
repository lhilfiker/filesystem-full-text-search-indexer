#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

bool Index::index_lock = false;
// This will indicate if the index is locked, either by our proccess or another
// one.
bool Index::read_only = true;
// This indicates if the index is readonly or not. It is readonly if the index
// is not locked or when index is locked but not by us.

int Index::lock_status() {
  // This will check for the presence of the lock file.
  // return values are -1 for not initialized, 0 for locked but readonly, 1 for
  // unlocked and readonly and 2 for locked and not read-only
  if (!initialized) {
    index_lock = false;
    read_only = true;
    return -1;
  }
  if (!std::filesystem::exists(CONFIG_INDEX_PATH / "index.lock")) {
    index_lock = false;
    read_only = true;
    return 1;
  }

  // read the lock file and compare it to current pid.

  // check if the pid is still running, if not delete file.

  return 0;
}

bool Index::lock() {
  if (lock_status() <= 0) {
    // not initialized or locked by other proccess.
    return false;
  }
}

bool Index::unlock() {}
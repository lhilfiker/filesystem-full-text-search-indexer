#include "../Logging/logging.h"
#include "index.h"
#include "index_types.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <string>
#include <unistd.h>

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

  pid_t pid = getpid(); // current pid

  std::ifstream lock_file(CONFIG_INDEX_PATH / "index.lock");
  if (!lock_file) {
    return -1; // Failed to open file
  }

  pid_t lockfile_pid;
  lock_file >> lockfile_pid;
  if (lockfile_pid == pid) {
    // our own pid's lockfile
    read_only = false;
    index_lock = true;
    return 2;
  }

  // check if the pid is still running, if not delete file.
  if (kill(lockfile_pid, 0) == 0) {
    // kill 0 doesn't actually kill the proccess, it just checks if the pid
    // exists. If it returns 0 the pid exists
    read_only = true;
    index_lock = true;
    return 0;

  } else {
    // we can remove the lock file
    std::filesystem::remove(CONFIG_INDEX_PATH / "index.lock");
    read_only = true;
    index_lock = false;
    return 1;
  }

  return 0;
}

bool Index::lock() {
  if (lock_status() <= 0) {
    // not initialized or locked by other proccess.
    return false;
  }
}

bool Index::unlock() {}
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
  Log::write(1, "Index: lock: trying to acquire lock");
  int lock_status_cached = lock_status();
  if (lock_status_cached <= 0) {
    // not initialized or locked by other proccess.
    Log::write(2,
               "Index: lock: Index not initialized or locked by other program");

    return false;
  } else if (lock_status_cached == 2) {
    Log::write(1, "Index: lock: we already locked it.");
    return true; // already locked.
  } else {
    pid_t pid = getpid(); // current pid
    std::ofstream lock_file(CONFIG_INDEX_PATH / "index.lock");
    if (!lock_file) {
      Log::write(1, "Index: lock: error creating lock file. Permission error?");
      return false; // Failed to create file
    }
    lock_file << pid;
    lock_file.close();
    if (lock_status() == 2) {
      Log::write(1, "Index: lock: successfully acquired the lock.");

      return true; // lock aquired.
    } else {
      Log::write(1, "Index: lock: created lock file but didn't get "
                    "confirmation. did not acquire lock.");

      return false; // some kind of error happend.
    }
  }
}

bool Index::unlock() {
  Log::write(1, "Index: unlock: trying to unlock");
  int lock_status_cached = lock_status();
  if (lock_status_cached <= 0) {
    // not initialized or locked by other proccess.
    Log::write(
        2, "Index: unlock: Index not initialized or locked by other program");

    return false;
  } else if (lock_status_cached == 1) {
    Log::write(1, "Index: unlock: we don't have an active lock.");
    return true; // not locked.
  } else {
    std::filesystem::remove(CONFIG_INDEX_PATH / "index.lock");
    Log::write(1, "Index: unlock: removed lock file.");
    if (lock_status() == 1) {
      return true;
    } else {
      Log::write(
          1,
          "Index: unlock: removed but still have the lock. Permission Error?");

      return false;
    }
  }
  return false;
}

bool Index::health_status() {
  // This will return wether or not the index is healthy. Meaning if it's
  // healthy you can search the Index. If it's not healthy you can't search it
  // because a first time add or transaction execution is in progress.
  // This will just check for a Transaction file or first time add file to
  // exist.

  if (std::filesystem::exists(CONFIG_INDEX_PATH / "firsttimewrite.info") ||
      std::filesystem::exists(CONFIG_INDEX_PATH / "transaction" /
                              "transaction.list")) {
    return false;
  } else {
    return true;
  }
}
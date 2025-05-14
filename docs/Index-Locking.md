# Index Locking Mechanism

## Overview
The Index Locking system provides file-based inter-process locking to prevent data corruption from concurrent writes. It uses a PID-based lock file and includes automatic stale lock detection.

## Locking States

- **-1**: Not initialized
- **0**: Locked by another process (read-only mode)
- **1**: Not locked (available for locking but currently read-only)
- **2**: Locked by current process (write mode)

## Core Functions

### Check Lock Status
```cpp
int Index::lock_status(bool initialize)
```
Returns the current lock state. If `initialize` is false, retries based on configured timeout. If true it skips the check to see if the Index is initialized and does not retry until available. Do not use this for anything other than initialization.

### Acquire Lock
```cpp
bool Index::lock(bool initialize)
```
Creates the lock file with the current process PID. Returns `true` if successful.

### Release Lock
```cpp
bool Index::unlock(bool initialize)
```
Removes the lock file. Returns `true` if successful.

### Check Index Health
```cpp
bool Index::health_status()
```
Returns `false` if a transaction or first-time write is in progress; otherwise `true`.

## Example Usage

```cpp
// Try to lock the index
if (Index::lock(false)) {
    // Perform write operations
    // ...
    
    // Release the lock when done
    Index::unlock(false);
} else {
    Log::write(3, "Could not acquire lock");
}

// For read operations, check health
if (Index::health_status()) {
    // Safe to perform searches
}
```

## Automatic Cleanup

The system automatically detects and removes stale locks:
```cpp
if (kill(lockfile_pid, 0) != 0) {
    // Process no longer exists, remove lock file
    std::filesystem::remove(CONFIG_INDEX_PATH / "index.lock");
}
```

## Configuration

- Lock acquisition timeout: `CONFIG_LOCK_ACQUISTION_TIMEOUT` (default: 30s)
- Lock file location: `CONFIG_INDEX_PATH/index.lock`

## Future Considerations
- Lock when doing searches to prevent writing while searches are in progress
- Retries not in lock status, in lock and unlock.
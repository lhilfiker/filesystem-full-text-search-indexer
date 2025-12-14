# Index Integrity Check Documentation

## Overview

The index integrity check system validates the correctness of all index files. The expensive check performs
comprehensive validation of the entire index structure and is designed to detect corruption rather than fix it.

## Usage

### Running the Expensive Check

```bash
# With verbose output (recommended)
./filesystem-indexer --check -v --config_file=/path/to/config.txt

# Without verbose output
./filesystem-indexer --check --config_file=/path/to/config.txt
```

### Prerequisites

- All transactions must be completed (no pending `transaction.list`)
- A full lock must be acquirable (no other processes using the index)
- Index must be initialized

## Check Types

All checks are **FATAL** - any failure indicates index corruption that requires a full reindex to resolve.

## Complete Check List

### Words Index Checks

| Check                | Description                                                  |
|----------------------|--------------------------------------------------------------|
| Separator validity   | All word separators match up and total size equals file size |
| Character validation | All non-separator bytes are lowercase a-z only               |
| Alphabetical order   | Words are sorted alphabetically                              |
| No duplicates        | No duplicate word entries exist                              |

### Paths Index Checks

| Check              | Description                                                  |
|--------------------|--------------------------------------------------------------|
| Separator validity | All path separators match up and total size equals file size |

### Paths Count Index Checks

| Check           | Description                        |
|-----------------|------------------------------------|
| Size validation | Size equals `path_count × 4` bytes |

### Words_F Index Checks

| Check              | Description                                                                   |
|--------------------|-------------------------------------------------------------------------------|
| Size validation    | Size equals `26 × (8 + WORDS_F_LOCATION_SIZE)` bytes                          |
| Reference accuracy | Each letter entry references the correct first word starting with that letter |

### Reversed Index Checks

| Check                  | Description                                                       |
|------------------------|-------------------------------------------------------------------|
| Size validation        | Size equals `word_count × REVERSED_ENTRY_SIZE` bytes              |
| Path ID validity       | All path IDs reference valid paths (1 to path_count)              |
| Additional ID validity | All additional IDs reference valid additional blocks              |
| Zero padding           | After a 0 path ID appears, only 0s follow in that block           |
| No duplicate paths     | No duplicate path IDs within a word's reversed + additional chain |

### Additional Index Checks

| Check                   | Description                                             |
|-------------------------|---------------------------------------------------------|
| Size divisibility       | Size is divisible by `ADDITIONAL_ENTRY_SIZE`            |
| Path ID validity        | All path IDs reference valid paths (1 to path_count)    |
| Additional ID validity  | All chained additional IDs are valid                    |
| Zero padding            | After a 0 path ID appears, only 0s follow in that block |
| No duplicate references | No additional block is referenced more than once        |

## Implementation Details

### Words Index Validation

The check iterates through `words.index` sequentially:

1. Reads the separator byte(s) to determine word length
2. Validates separator is non-zero and doesn't exceed file bounds
3. Reads each character, validating it's within a-z range
4. Compares with previous word to ensure alphabetical ordering and no duplicates
5. Tracks first occurrence of each letter for words_f comparison
6. Verifies final position matches file size exactly

### Words_F Index Validation

After words validation completes:

1. Verifies size equals `26 × (8 + WORDS_F_LOCATION_SIZE)` bytes
2. Copies words_f into memory for comparison
3. Validates letter 'a' entry is at location 0 with id 0
4. For each letter, compares against values built during words scan
5. For letters with no words, validates they point to next non-empty letter's position

### Paths Index Validation

Iterates through `paths.index`:

1. Reads 2-byte path length separator
2. Validates separator doesn't exceed remaining file size
3. Advances position by separator value
4. Counts total paths for later validation
5. Verifies final position matches file size exactly

### Paths Count Validation

Simple size check: `paths_count_size == path_count × 4`

### Reversed & Additional Validation

The most complex validation, performed together:

1. Validates reversed size is divisible by `REVERSED_ENTRY_SIZE`
2. Validates additional size is divisible by `ADDITIONAL_ENTRY_SIZE` (or is 0)
3. Validates reversed size equals `word_count × REVERSED_ENTRY_SIZE`
4. For each reversed block:
    - Tracks used path IDs to detect duplicates
    - Validates all path IDs are within valid range (1 to path_count)
    - Detects gaps (0 followed by non-zero)
    - Follows additional chain if present
5. For each additional block in chain:
    - Validates additional ID is within bounds
    - Tracks used additional IDs to detect duplicate references
    - Performs same path ID validation as reversed
    - Follows next additional link until 0

## Output Examples

### Successful Check (Verbose)

```
Check: Index seems to be correct. No corruption or any sign of corruption detected.
```

### Failed Check Examples

```
Check: Words Index: Word seperator is lower or equal to 0 which is invalid. Index most likely corrupt.
```

```
Check: Words Index: Word is smaller than previous word or the same. Index most likely corrupt.
```

```
Check: Reversed entry has a duplicate path id.
```

```
Check: Additional block linked more than once.
```

## Limitations

- Detection only, no automatic repair
- Requires write lock (blocks other operations)
- Expensive for large indexes (reads entire index)
- Cannot detect all forms of corruption (e.g., valid but incorrect data)

## Future Improvements

- Fast check mode for common corruption patterns
- Automatic repair for recoverable corruptions
- Checksum validation
- Partial index verification
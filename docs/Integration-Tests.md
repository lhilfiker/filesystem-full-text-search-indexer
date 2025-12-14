# Integration Testing Documentation

## Overview

The integration test suite validates the complete indexing and search workflow, ensuring data integrity across indexing,
merging, and searching operations. Tests run automatically via GitHub Actions on every push and pull request.

## Test Structure

### Test Phases

The test suite executes six sequential phases:

| Phase               | Purpose                         | Key Validations                             |
|---------------------|---------------------------------|---------------------------------------------|
| 1. Initial Indexing | First-time index creation       | Index files created, integrity check passes |
| 2. Merge Content    | Add new files to existing index | Merge completes, integrity preserved        |
| 3. Search Tests     | Validate search functionality   | Wildcard, exact match, query operators      |
| 4. Dynamic Content  | Add and find new content        | New files indexed and searchable            |
| 5. Fresh Full Index | Clean rebuild                   | Full reindex works correctly                |
| 6. Edge Cases       | Handle special files            | Empty files, whitespace, special characters |

### Test Data

Test data consists of Project Gutenberg text files stored in Git LFS:

- `tests/data1.zip` - Initial dataset (100 files)
- `tests/data2.zip` - Merge dataset (288 files)

Files are automatically extracted during test execution.

## Running Tests

### CI Execution

Tests run automatically on GitHub Actions for:

- All pushes to any branch
- All pull requests

## Test Categories

### Indexing Tests

- First-time index creation from empty state
- Incremental merge of additional files
- Full reindex from scratch
- Edge case file handling (empty, whitespace-only, special characters)

### Integrity Tests

After each indexing operation, the expensive  [Index-Checks.md](Index-Checks.md) (`--check -v`) validates:

- Word index structure and alphabetical ordering
- Path index separator correctness
- Reversed index size matches word count
- Words_f jump table accuracy
- No index corruption detected

### Search Tests

Search validation covers the complete query language:

**Basic Search (5+ character minimum)**

```bash
./filesystem-indexer 'which'      # Wildcard search
./filesystem-indexer '"people"'   # Exact match
```

**Boolean Operators**

```bash
./filesystem-indexer '(which AND would)'           # Both terms required
./filesystem-indexer '(xyzzy OR which)'            # Either term matches
./filesystem-indexer '(which NOT xyzzyqwk)'        # Exclude term
./filesystem-indexer '((which OR would) AND there)' # Nested query
```

### Dynamic Content Tests

Validates that newly added content becomes searchable:

1. Create file with unique word not in test corpus
2. Run indexer to add new file
3. Verify integrity check passes
4. Search for unique word returns new file

## Test Configuration

Tests use a temporary configuration file with these settings:

```
index_path=/tmp/fts-test-XXXXX/index
config_path_to_scan=/tmp/fts-test-XXXXX/scan_part1
config_threads_to_use=2
config_local_index_memory=50000000
config_min_log_level=2
config_updated_files_only=false
```

## Test Output

### Success Output

```
╔════════════════════════════════════════════╗
║              Test Results                  ║
╚════════════════════════════════════════════╝
Tests run:    20
Tests passed: 20
Tests failed: 0

All tests passed!
```

### Failure Handling

- **Fatal tests**: Indexing and integrity checks stop execution on failure
- **Non-fatal tests**: Search tests continue to run, failures are reported at end
- **Artifact upload**: On CI failure, test directory contents are uploaded for debugging

## Adding New Tests

### Test Helper Functions

```bash
# Run test, continue on failure
run_test "Test name" "command"

# Run test, stop execution on failure
run_test_fatal "Test name" "command"

# Assert command exits with specific code
assert_exit_code 0 ./filesystem-indexer --help

# Assert output contains pattern
assert_output_contains 'pattern' ./filesystem-indexer 'query'

# Assert output does not contain pattern
assert_output_not_contains 'pattern' ./filesystem-indexer 'query'
```

### Example: Adding a New Search Test

```bash
run_test "Search with new operator" \
    "assert_output_contains '/' $INDEXER_BIN --config_file=$CONFIG_FILE '(word1 AND word2)'"
```

## Limitations

- No crash recovery testing (transaction replay not yet implemented)
- No concurrent access testing
- No performance benchmarking

## Future Improvements

- Transaction recovery testing after simulated crashes
- Multi-process concurrent access tests
- Performance regression tests
- Fuzz testing for query parser
#!/bin/bash
# Integration test suite for filesystem-full-text-search-indexer
set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INDEXER_BIN="${1:-./build/filesystem-indexer}"
TEST_DATA1_DIR="$SCRIPT_DIR/data1"
TEST_DATA2_DIR="$SCRIPT_DIR/data2"
TEST_WORK_DIR=$(mktemp -d /tmp/fts-test-XXXXXX)
INDEX_PATH="$TEST_WORK_DIR/index"
CONFIG_FILE="$TEST_WORK_DIR/config.txt"

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

cleanup() {
    echo -e "\n${YELLOW}Cleaning up test directory: $TEST_WORK_DIR${NC}"
    rm -rf "$TEST_WORK_DIR"
}

trap cleanup EXIT

log_info() { echo -e "${YELLOW}[INFO]${NC} $1"; }
log_pass() { echo -e "${GREEN}[PASS]${NC} $1"; ((TESTS_PASSED++)); }
log_fail() { echo -e "${RED}[FAIL]${NC} $1"; ((TESTS_FAILED++)); }

run_test() {
    local name="$1"
    local cmd="$2"
    ((TESTS_RUN++))

    echo -e "\n${YELLOW}━━━ Test: $name ━━━${NC}"
    echo "Running: $cmd"
    set +e
    eval "$cmd"
    local result=$?
    set -e
    if [ $result -eq 0 ]; then
        log_pass "$name"
        return 0
    else
        log_fail "$name"
        echo "Command output (if any):"
        cat "$TEST_WORK_DIR/last_output.txt" 2>/dev/null || true
        return 1
    fi
}

assert_exit_code() {
    local expected="$1"
    shift
    local actual
    set +e
    "$@" > "$TEST_WORK_DIR/last_output.txt" 2>&1
    actual=$?
    set -e
    if [ "$actual" -eq "$expected" ]; then
        return 0
    else
        echo "Expected exit code $expected, got $actual"
        echo "Output:"
        cat "$TEST_WORK_DIR/last_output.txt"
        return 1
    fi
}

assert_output_contains() {
    local pattern="$1"
    shift
    set +e
    "$@" > "$TEST_WORK_DIR/last_output.txt" 2>&1
    local exit_code=$?
    set -e
    if [ $exit_code -ne 0 ]; then
        echo "Command failed with exit code $exit_code"
        cat "$TEST_WORK_DIR/last_output.txt"
        return 1
    fi
    if grep -q "$pattern" "$TEST_WORK_DIR/last_output.txt"; then
        return 0
    else
        echo "Pattern '$pattern' not found in output:"
        cat "$TEST_WORK_DIR/last_output.txt"
        return 1
    fi
}

assert_output_not_contains() {
    local pattern="$1"
    shift
    set +e
    "$@" > "$TEST_WORK_DIR/last_output.txt" 2>&1
    set -e
    if grep -q "$pattern" "$TEST_WORK_DIR/last_output.txt"; then
        echo "Pattern '$pattern' unexpectedly found in output:"
        cat "$TEST_WORK_DIR/last_output.txt"
        return 1
    fi
    return 0
}

create_config() {
    local scan_path="$1"
    cat > "$CONFIG_FILE" << EOF
index_path=$INDEX_PATH
config_path_to_scan=$scan_path
config_threads_to_use=2
config_local_index_memory=50000000
config_min_log_level=2
config_updated_files_only=false
EOF
}

# Verify indexer binary exists
if [ ! -x "$INDEXER_BIN" ]; then
    echo -e "${RED}Error: Indexer binary not found or not executable: $INDEXER_BIN${NC}"
    exit 1
fi

# Unzip test data if needed
if [ ! -d "$TEST_DATA1_DIR" ] && [ -f "$SCRIPT_DIR/data1.zip" ]; then
    echo "Extracting data1.zip..."
    unzip -q "$SCRIPT_DIR/data1.zip" -d "$SCRIPT_DIR/"
fi
if [ ! -d "$TEST_DATA2_DIR" ] && [ -f "$SCRIPT_DIR/data2.zip" ]; then
    echo "Extracting data2.zip..."
    unzip -q "$SCRIPT_DIR/data2.zip" -d "$SCRIPT_DIR/"
fi

# Verify test data exists
if [ ! -d "$TEST_DATA1_DIR" ]; then
    echo -e "${RED}Error: Test data directory not found: $TEST_DATA1_DIR${NC}"
    exit 1
fi
if [ ! -d "$TEST_DATA2_DIR" ]; then
    echo -e "${RED}Error: Test data directory not found: $TEST_DATA2_DIR${NC}"
    exit 1
fi

echo -e "${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Filesystem Indexer Integration Tests      ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo "Indexer: $INDEXER_BIN"
echo "Test data1: $TEST_DATA1_DIR"
echo "Test data2: $TEST_DATA2_DIR"
echo "Work dir: $TEST_WORK_DIR"

# Setup test directories
mkdir -p "$INDEX_PATH"
mkdir -p "$TEST_WORK_DIR/scan_part1"
mkdir -p "$TEST_WORK_DIR/scan_part2"
mkdir -p "$TEST_WORK_DIR/scan_full"

# Debug: show what we found
echo "Files in data1: $(ls -1 "$SCRIPT_DIR/data1/" 2>/dev/null | wc -l) files"
echo "Files in data2: $(ls -1 "$SCRIPT_DIR/data2/" 2>/dev/null | wc -l) files"

# Copy test data for partial indexing
cp "$SCRIPT_DIR/data1/"*.txt "$TEST_WORK_DIR/scan_part1/"
cp "$SCRIPT_DIR/data2/"*.txt "$TEST_WORK_DIR/scan_part2/"
cp "$SCRIPT_DIR/data1/"*.txt "$TEST_WORK_DIR/scan_full/"
cp "$SCRIPT_DIR/data2/"*.txt "$TEST_WORK_DIR/scan_full/"

echo "Files copied to scan_part1: $(ls -1 "$TEST_WORK_DIR/scan_part1/" | wc -l) files"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 1: Initial Indexing
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 1: Initial Indexing ═══${NC}"

create_config "$TEST_WORK_DIR/scan_part1"

run_test "Help command works" \
    "assert_exit_code 0 $INDEXER_BIN --help"

run_test "Index first batch of files" \
    "assert_exit_code 0 $INDEXER_BIN -i --config_file=$CONFIG_FILE"

run_test "Index files exist after indexing" \
    "[ -f '$INDEX_PATH/words.index' ] && [ -f '$INDEX_PATH/paths.index' ] && [ -f '$INDEX_PATH/reversed.index' ]"

run_test "Run expensive index check after initial index" \
    "assert_output_contains 'No corruption' $INDEXER_BIN --check -v --config_file=$CONFIG_FILE"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 2: Merge Additional Content (data2)
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 2: Merge Additional Content ═══${NC}"

# Add data2 files to scan directory for merge
cp "$SCRIPT_DIR/data2/"*.txt "$TEST_WORK_DIR/scan_part1/"

run_test "Merge additional files into index" \
    "assert_exit_code 0 $INDEXER_BIN -i --config_file=$CONFIG_FILE"

run_test "Run expensive index check after merge" \
    "assert_output_contains 'No corruption' $INDEXER_BIN --check -v --config_file=$CONFIG_FILE"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 3: Search Tests
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 3: Search Tests ═══${NC}"

# Search for word that should NOT exist
NONEXISTENT_WORD="xyzzyqwkplm"

run_test "Search for non-existent word returns no results" \
    "assert_output_not_contains 'book' $INDEXER_BIN --config_file=$CONFIG_FILE '$NONEXISTENT_WORD'"

# Search for common word that should exist (from Gutenberg books)
run_test "Search for common word 'the' returns results" \
    "assert_output_contains '/' $INDEXER_BIN --config_file=$CONFIG_FILE 'the'"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 4: Dynamic Content Test
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 4: Dynamic Content Test ═══${NC}"

# Create a new file with a unique word
UNIQUE_WORD="supercalifragilisticexpialidocious"
echo "This file contains the unique word $UNIQUE_WORD for testing purposes." > "$TEST_WORK_DIR/scan_part1/unique_test.txt"
echo "The word $UNIQUE_WORD appears multiple times here." >> "$TEST_WORK_DIR/scan_part1/unique_test.txt"

run_test "Index new file with unique word" \
    "assert_exit_code 0 $INDEXER_BIN -i --config_file=$CONFIG_FILE"

run_test "Run expensive index check after adding new file" \
    "assert_output_contains 'No corruption' $INDEXER_BIN --check -v --config_file=$CONFIG_FILE"

run_test "Search finds unique word in new file" \
    "assert_output_contains 'unique_test.txt' $INDEXER_BIN --config_file=$CONFIG_FILE '$UNIQUE_WORD'"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 5: Fresh Full Index
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 5: Fresh Full Index ═══${NC}"

# Clean index and reindex everything
rm -rf "$INDEX_PATH"
mkdir -p "$INDEX_PATH"
create_config "$TEST_WORK_DIR/scan_full"

run_test "Full index from scratch" \
    "assert_exit_code 0 $INDEXER_BIN -i --config_file=$CONFIG_FILE"

run_test "Run expensive index check on full index" \
    "assert_output_contains 'No corruption' $INDEXER_BIN --check -v --config_file=$CONFIG_FILE"

# ═══════════════════════════════════════════════════════════════
# TEST PHASE 6: Edge Cases
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}═══ Phase 6: Edge Cases ═══${NC}"

# Create empty file
touch "$TEST_WORK_DIR/scan_full/empty.txt"

# Create file with only whitespace
echo "     " > "$TEST_WORK_DIR/scan_full/whitespace.txt"

# Create file with special characters
echo "Test with special chars: @#$%^&*()!" > "$TEST_WORK_DIR/scan_full/special.txt"

run_test "Index handles edge case files" \
    "assert_exit_code 0 $INDEXER_BIN -i --config_file=$CONFIG_FILE"

run_test "Index check passes after edge case files" \
    "assert_output_contains 'No corruption' $INDEXER_BIN --check -v --config_file=$CONFIG_FILE"

# ═══════════════════════════════════════════════════════════════
# Results Summary
# ═══════════════════════════════════════════════════════════════
echo -e "\n${GREEN}╔════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              Test Results                  ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════╝${NC}"
echo -e "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi
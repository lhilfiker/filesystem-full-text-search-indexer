# Index Searching Documentation

## Overview
The Search system provides efficient full-text search across indexed files with configurable matching preferences, processing queries to retrieve and rank relevant documents.

## Configuration Parameters
- **Exact Match**: When `true`, only exact word matches are considered; when `false`, partial matches are allowed based on minimum character threshold
- **Minimum Character Match**: When exact match is disabled, sets the minimum number of matching characters required (default: 4)

```cpp
// Example configuration
Search::save_config(false, 3);  // Allow partial matches with at least 3 matching characters
```

## Search Process

### 1. Query Processing
- Input is split into words by whitespace and other separators
- Characters are normalized through `helper::convert_char()`
- Words are filtered for length (must be between 2-100 characters(will be changed later))

### 2. Word Index Search
The `Index::search_word_list` function performs the core search:
- Takes search terms vector and matching parameters
- Uses `words_f.index` for efficient first-letter jumps
- Compares words character-by-character
- Returns a vector of `search_path_ids_return` structs containing path IDs and match counts

### 3. Path ID Retrieval and Mapping
- For each matching word ID, retrieves associated path IDs from `reversed.index`
- For words with more than 4 occurrences, follows chains in `additional.index`
- Builds a frequency map of path occurrences

### 4. Path String Lookup
The `Index::id_to_path_string` function:
- Converts numeric path IDs to file paths by reading from `paths.index`
- Returns an unordered map mapping path IDs to path strings
- Optimizes lookup by sorting IDs before reading

### 5. Result Ranking
- Results are sorted by match count in descending order
- Each result displays path and match count

## Implementation Notes

### Key Functions

```cpp
// Core search functions
std::vector<search_path_ids_return> Index::search_word_list(
    std::vector<std::string> &search_words, 
    bool exact_match, 
    int min_char_for_match);

std::unordered_map<uint64_t, std::string> Index::id_to_path_string(
    std::vector<search_path_ids_return> path_ids);

// Search entry point
void Search::search();
```

### Performance Considerations
- Memory-mapped files avoid loading the entire index into memory
- `words_f.index` provides O(1) jumps to starting position of each letter
- `search_path_ids_return` struct optimizes result sorting
- Path ID retrieval uses direct index lookups: `(word_id * 10)` for reversed blocks

### Memory Usage
- Search operation maintains minimal temporary structures
- Results are built incrementally to minimize memory impact
- Path strings are only loaded for matching documents

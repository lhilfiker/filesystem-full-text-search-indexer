# filesystem-full-text-search-indexer

A high-performance, multithreaded file indexer with custom binary storage format designed for minimal memory usage and maximum scalability. Features transaction-based updates with crash recovery, inter-process locking for data integrity, and support for millions of files. Includes a powerful boolean query language (AND, OR, NOT) with wildcard and exact matching capabilities, optimized search performance through alphabet jump tables, and configurable memory settings for different dataset sizes.

## Project Status

**Alpha Release** - The project is currently in alpha state but already works quite well for most use cases. While still in active development, the core functionality is stable and ready for testing and feedback.

## Features

### Core Functionality
- **Full-text indexing** of text files (.txt, .md) with word-based search
- **Multithreaded processing** for efficient indexing of large file collections
- **Boolean query language** supporting AND, OR, NOT operators with nested parentheses
- **Wildcard and exact matching** with configurable minimum character requirements
- **Incremental updates** - detect and index only changed files since last scan
- **Interactive search mode** for real-time querying

### Advanced Features
- **Self-healing index structure** with transaction-based updates ensuring data integrity
- **Inter-process locking** prevents corruption during concurrent access
- **Memory-mapped file I/O** for efficient disk access and minimal memory usage
- **Optimized binary storage** with configurable data types for different dataset sizes
- **Alphabet jump tables** for fast search optimization
- **Crash recovery** - automatic recovery from interrupted operations
- **Configurable memory limits** and threading options

### Technical Highlights
- **Custom index format** designed for minimal disk footprint
- **Transaction system** ensures index consistency even during system failures
- **Scalable architecture** supporting millions of files and gigabytes of content
- **Configuration system** with file-based and command-line options
- **Comprehensive logging** with configurable log levels

## Installation

### Option 1: Release Downloads
Download the latest release from the [GitHub releases page](https://github.com/lhilfiker/filesystem-full-text-search-indexer/releases) and extract the binary to your preferred location.

### Option 2: Build from Source (Recommended)

#### Quick Build
```bash
./build.sh
```
The build script will check dependencies and build the project automatically.

#### Manual Build
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### Debug Build
For development or debugging:
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Dependencies
- CMake 3.16 or higher
- C++17 compatible compiler (GCC, Clang)
- Build tools (make, build-essential on Ubuntu/Debian)

## Usage

### Basic Commands
```bash
# Show help
./filesystem-indexer --help

# Index files in a directory
./filesystem-indexer -i --index_path=/path/to/index --config_path_to_scan=/path/to/documents

# Interactive search mode
./filesystem-indexer -s --index_path=/path/to/index

# Direct search
./filesystem-indexer --index_path=/path/to/index "search query"
```

### Configuration
Create a configuration file at `~/.config/filesystem-full-text-search-indexer/config.txt`:

```
index_path=/home/user/.local/share/filesystem-indexer
config_path_to_scan=/home/user/documents
config_threads_to_use=4
config_local_index_memory=100000000
```

See [docs/Config-Values.md](docs/Config-Values.md) for all available configuration options.

### Query Examples
```bash
# Simple wildcard search
apple

# Boolean operations
(apple AND banana)
(fruit OR vegetable)
(apple NOT pie)

# Exact matching
("exact phrase")

# Complex queries
((fruit OR vegetable) AND "fresh")
```

## Documentation

- [Configuration System](docs/Config-Values.md) - All configuration options and formats
- [Query Language](docs/Query-Language.md) - Complete search syntax guide
- [Disk Structure](docs/Disk-Structure.md) - Technical details of index format
- [Multi-Threading](docs/Multi-Threading.md) - Threading and memory management
- [Transaction System](docs/Merging.md) - How data integrity is maintained

## Development Status

The project has reached alpha stability with the completion of milestone v0.1. Current development focuses on:

**Completed for v0.1:**
- Core indexing and search functionality
- Configuration system with file and CLI support
- Incremental indexing (updated files only)
- Transaction-based updates with crash recovery
- Build system and documentation

**Planned for v0.2:**
- Enhanced maintainability and refactoring
- Comprehensive test coverage
- Performance optimizations for TB-scale datasets
- Additional file format support
- Metadata indexing per file

Development roadmap and issues are tracked in the [project milestones](https://github.com/lhilfiker/filesystem-full-text-search-indexer/milestones).

## System Requirements

- **Architecture**: Little-endian systems (x86, x86_64)
- **CPU**: 64-bit processor (32-bit may work but untested)
- **Memory**: Configurable, depends on dataset size and settings
- **Storage**: Sufficient space for index files (typically 15% of indexed content size)
- **OS**: only Linux currently

## Contributing

This project is in active development. Contributions, bug reports, and feedback are welcome through GitHub issues and pull requests.

## License

MIT License - see [LICENSE](LICENSE) file for details.
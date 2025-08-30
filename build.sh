#!/bin/bash

# Simple build script for filesystem-full-text-search-indexer
set -e

echo "Building filesystem-full-text-search-indexer..."

# Check dependencies
if ! command -v cmake >/dev/null 2>&1; then
    echo "Error: CMake not found. Please install:"
    echo "  Ubuntu/Debian: sudo apt install cmake build-essential"
    echo "  Fedora/RHEL: sudo dnf install cmake gcc-c++"
    exit 1
fi

if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    echo "Error: No C++ compiler found. Please install build tools."
    exit 1
fi

# Check build directory
if [ -d "build" ] && [ "$(ls -A build 2>/dev/null)" ]; then
    echo "Build directory exists. Delete it? (y/N)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        rm -rf build
        echo "Build directory cleaned."
    fi
fi

# Build
echo "Building..."
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc 2>/dev/null || echo 2)

echo
echo "Build complete!"
echo
echo "Do you want to install the program in /usr/local/bin? (y/N)"
read -r response
if [[ "$response" =~ ^[Yy]$ ]]; then
    sudo make install
fi
echo
echo "Done!"
echo
echo "Quick start:"
echo "  1. Create config directory: mkdir -p ~/.config/filesystem-full-text-search-indexer/"
echo "  2. Copy example config: cp /usr/local/share/doc/filesystem-full-text-search-indexer/config.txt.example ~/.config/filesystem-full-text-search-indexer/config.txt"
echo "  3. Edit the config file with your paths"
echo "  4. Run: filesystem-indexer --help   to get started."
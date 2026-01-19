#!/bin/bash
set -e

echo "========================================"
echo "GemStack Build Script"
echo "========================================"
echo

# Check for required tools
check_command() {
    if ! command -v "$1" &> /dev/null; then
        echo "[ERROR] $1 is not installed or not in PATH"
        exit 1
    fi
}

check_command git
check_command node
check_command npm
check_command cmake

# Step 1: Initialize/update submodules
echo "[1/4] Initializing submodules..."
git submodule update --init --recursive
echo "[OK] Submodules initialized"
echo

# Step 2: Install Gemini CLI dependencies
echo "[2/4] Installing Gemini CLI dependencies..."
cd gemini-cli
npm install
echo "[OK] Dependencies installed"
echo

# Step 3: Build Gemini CLI
echo "[3/4] Building Gemini CLI..."
npm run build
cd ..
echo "[OK] Gemini CLI built"
echo

# Step 4: Build GemStack
echo "[4/4] Building GemStack..."
cmake -B build -S .
cmake --build build
echo "[OK] GemStack built"
echo

echo "========================================"
echo "Build completed successfully!"
echo
echo "Run GemStack with:"
echo "  ./build/GemStack"
echo "========================================"

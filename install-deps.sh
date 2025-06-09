#!/bin/bash

# OutlineTool Dependency Installation Script
# This script helps install libdxfrw which is required for DXF export

set -e

echo "Installing OutlineTool dependencies..."

# Check if we're on macOS
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Error: Homebrew is required but not installed."
        echo "Please install Homebrew from https://brew.sh"
        exit 1
    fi
    
    # Install OpenCV if not present
    if ! brew list opencv &> /dev/null; then
        echo "Installing OpenCV..."
        brew install opencv
    else
        echo "✓ OpenCV already installed"
    fi
    
    # Install build tools
    echo "Installing build tools..."
    brew install cmake git
    
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux"
    
    # Update package list
    sudo apt update
    
    # Install dependencies
    echo "Installing build dependencies..."
    sudo apt install -y build-essential cmake git libopencv-dev
    
else
    echo "Unsupported operating system: $OSTYPE"
    exit 1
fi

# Install libdxfrw from source
echo ""
echo "Installing libdxfrw from source..."

# Create temporary directory
TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Clone the repository
echo "Cloning libdxfrw..."
git clone https://github.com/aewallin/libdxfrw.git
cd libdxfrw

# Build and install
echo "Building libdxfrw..."
mkdir -p build
cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "Installing libdxfrw..."
sudo make install

# Update library cache on Linux
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    sudo ldconfig
fi

# Clean up
cd /
rm -rf "$TEMP_DIR"

echo ""
echo "✓ Dependencies installed successfully!"
echo ""
echo "You can now build OutlineTool with:"
echo "  make build"
echo ""
echo "If you encounter any issues, you may need to specify library paths:"
echo "  cmake -DDXFRW_LIBRARY=/usr/local/lib/libdxfrw.so -DDXFRW_INCLUDE_DIR=/usr/local/include"
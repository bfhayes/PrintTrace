#!/bin/bash

# PrintTrace Dependency Installation Script
# This script installs the minimal dependencies needed for PrintTrace
# Note: libdxfrw is now included as a submodule and built automatically

set -e

echo "Installing PrintTrace dependencies..."

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

echo ""
echo "✓ Dependencies installed successfully!"
echo ""
echo "libdxfrw is included as a submodule and will be built automatically."
echo ""
echo "Next steps:"
echo "  1. Clone the repository with submodules:"
echo "     git clone --recursive https://github.com/user/PrintTrace.git"
echo "  2. Build the library:"
echo "     cd PrintTrace && make lib"
echo "  3. Or if you already have the repo:"
echo "     git submodule update --init --recursive && make lib"
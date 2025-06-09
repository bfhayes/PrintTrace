# PrintTrace Makefile
# Simple wrapper around CMake for easier building

.PHONY: all build clean install debug release test help lib dylib cli tool executable install-lib

# Default target
all: lib

# Build the project (Release mode by default)
build:
	@echo "Building PrintTrace..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Build complete! Executable: build/PrintTrace"

# Debug build
debug:
	@echo "Building PrintTrace (Debug)..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Debug build complete! Executable: build/PrintTrace"

# Release build (explicit)
release:
	@echo "Building PrintTrace (Release)..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Release build complete! Executable: build/PrintTrace"

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "Clean complete."

# Install the built executable
install: build
	@echo "Installing PrintTrace..."
	@cd build && make install
	@echo "Installation complete."

# Build shared library only (primary target)
lib dylib:
	@echo "Building PrintTrace shared library..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXECUTABLE=OFF -DBUILD_SHARED_LIB=ON
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "✅ Shared library complete! Library: build/libprinttrace.dylib"
	@echo "📋 Header available at: include/PrintTraceAPI.h"

# Install library to system (requires sudo)
install-lib: lib
	@echo "Installing PrintTrace library to system..."
	@cd build && sudo make install
	@echo "✅ Library installed! Now available via pkg-config:"
	@echo "   pkg-config --cflags --libs printtrace"

# Build CLI tool (uses shared library)
cli tool:
	@echo "Building PrintTrace CLI tool..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CLI_TOOL=ON -DBUILD_SHARED_LIB=ON -DBUILD_EXECUTABLE=OFF
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "✅ CLI tool complete! Binary: build/printtrace"
	@echo "📋 Uses shared library: build/libprinttrace.dylib"

# Build executable only (monolithic)
executable:
	@echo "Building PrintTrace executable (monolithic)..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXECUTABLE=ON -DBUILD_SHARED_LIB=OFF -DBUILD_CLI_TOOL=OFF
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "✅ Executable complete! Binary: build/PrintTrace"

# Test build (build and run basic test)
test: build
	@echo "Testing PrintTrace..."
	@if [ -x build/PrintTrace ]; then \
		echo "✓ Executable built successfully"; \
		build/PrintTrace --help 2>/dev/null || build/PrintTrace || echo "✓ Binary runs"; \
	else \
		echo "✗ Executable not found"; \
		exit 1; \
	fi
	@if [ -f build/libprinttrace.dylib ]; then \
		echo "✓ Shared library built successfully"; \
		file build/libprinttrace.dylib; \
	else \
		echo "✗ Shared library not found"; \
	fi

# Configure with custom paths (example)
configure:
	@echo "Configuring with custom paths..."
	@mkdir -p build
	@echo "Example configuration commands:"
	@echo "  cd build && cmake .. -DOpenCV_DIR=/path/to/opencv"
	@echo "  cd build && cmake .. -DDXFRW_LIBRARY=/path/to/libdxfrw.so -DDXFRW_INCLUDE_DIR=/path/to/headers"

# Show help
help:
	@echo "PrintTrace Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Build shared library (default)"
	@echo "  lib/dylib   - Build shared library for Swift/C integration"
	@echo "  cli/tool    - Build CLI tool that uses shared library"
	@echo "  executable  - Build monolithic executable (all-in-one)"
	@echo "  install-lib - Install library to system (requires sudo)"
	@echo "  build       - Build both executable and library"
	@echo "  debug       - Build in Debug mode"
	@echo "  release     - Build in Release mode (explicit)"
	@echo "  clean       - Remove build directory"
	@echo "  install     - Install everything to system"
	@echo "  test        - Build and test library"
	@echo "  configure   - Show example configuration commands"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - OpenCV (core, imgproc, imgcodecs)"
	@echo "  - libdxfrw (included as submodule)"
	@echo ""
	@echo "Usage examples:"
	@echo "  make lib                      # Build shared library (default)"
	@echo "  make cli                      # Build CLI tool using shared library"
	@echo "  make install-lib              # Install library system-wide"
	@echo "  make executable               # Build standalone executable"
	@echo "  ./build/printtrace -i input.jpg -o output.dxf"
	@echo ""
	@echo "Library Integration:"
	@echo "  C/C++:   gcc -lprinttrace myapp.c"
	@echo "  Swift:   Use in Swift Package Manager (see docs)"
	@echo "  CMake:   find_package(PrintTrace REQUIRED)"
	@echo "  pkg-config: pkg-config --cflags --libs printtrace"
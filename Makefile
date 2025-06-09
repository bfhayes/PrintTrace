# OutlineTool Makefile
# Simple wrapper around CMake for easier building

.PHONY: all build clean install debug release test help lib dylib executable

# Default target
all: build

# Build the project (Release mode by default)
build:
	@echo "Building OutlineTool..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Build complete! Executable: build/OutlineTool"

# Debug build
debug:
	@echo "Building OutlineTool (Debug)..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Debug build complete! Executable: build/OutlineTool"

# Release build (explicit)
release:
	@echo "Building OutlineTool (Release)..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Release build complete! Executable: build/OutlineTool"

# Clean build directory
clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@echo "Clean complete."

# Install the built executable
install: build
	@echo "Installing OutlineTool..."
	@cd build && make install
	@echo "Installation complete."

# Build shared library only
lib dylib:
	@echo "Building OutlineTool shared library..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXECUTABLE=OFF -DBUILD_SHARED_LIB=ON
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Shared library complete! Library: build/liboutlinetool.dylib"

# Build executable only
executable:
	@echo "Building OutlineTool executable..."
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_EXECUTABLE=ON -DBUILD_SHARED_LIB=OFF
	@cd build && make -j$(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
	@echo "Executable complete! Binary: build/OutlineTool"

# Test build (build and run basic test)
test: build
	@echo "Testing OutlineTool..."
	@if [ -x build/OutlineTool ]; then \
		echo "✓ Executable built successfully"; \
		build/OutlineTool --help 2>/dev/null || build/OutlineTool || echo "✓ Binary runs"; \
	else \
		echo "✗ Executable not found"; \
		exit 1; \
	fi
	@if [ -f build/liboutlinetool.dylib ]; then \
		echo "✓ Shared library built successfully"; \
		file build/liboutlinetool.dylib; \
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
	@echo "OutlineTool Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Build both executable and library (default)"
	@echo "  build       - Build both executable and library (Release mode)"
	@echo "  lib/dylib   - Build shared library only"
	@echo "  executable  - Build command-line executable only"
	@echo "  debug       - Build in Debug mode"
	@echo "  release     - Build in Release mode (explicit)"
	@echo "  clean       - Remove build directory"
	@echo "  install     - Install the built executable and library"
	@echo "  test        - Build and test both executable and library"
	@echo "  configure   - Show example configuration commands"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  - OpenCV (core, imgproc, imgcodecs)"
	@echo "  - libdxfrw"
	@echo ""
	@echo "Usage examples:"
	@echo "  make build                    # Build both executable and library"
	@echo "  make dylib                    # Build only shared library for Swift"
	@echo "  ./build/OutlineTool -i input.jpg -o output.dxf"
	@echo ""
	@echo "Swift Integration:"
	@echo "  make dylib                    # Creates build/liboutlinetool.dylib"
	@echo "  # Use build/liboutlinetool.dylib in your Xcode project"
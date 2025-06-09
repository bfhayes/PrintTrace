# OutlineTool

A high-performance C++ library for extracting object outlines from images and exporting them as DXF vector files. Designed for easy integration into other applications, with a clean C API perfect for Swift Package Manager integration.

## Features

- **High-performance C++ core** - Optimized image processing algorithms
- **Clean C API** - Perfect for Swift, Python, and other language bindings
- **Automatic perspective correction** - Detects document/lightbox boundaries
- **Advanced noise reduction** - Morphological operations and Gaussian filtering
- **Real-world scaling** - Accurate pixel-to-millimeter conversion
- **DXF export** - Industry-standard CAD-compatible vector format
- **Easy integration** - pkg-config, CMake, and Swift Package Manager support
- **Cross-platform** - Works on macOS, Linux, and Windows

## Prerequisites

### Required Dependencies

- **CMake** (>= 3.16)
- **C++ Compiler** with C++17 support (GCC, Clang, or MSVC)
- **OpenCV** (>= 4.0) with core, imgproc, and imgcodecs modules
- **Git** (for submodule support)

**Note**: libdxfrw is now included as a submodule and built automatically - no manual installation required!

### Quick Installation

#### macOS (with Homebrew)

```bash
# Install dependencies
brew install cmake opencv git

# Clone with submodules
git clone --recursive https://github.com/user/OutlineTool.git
cd OutlineTool

# Build library
make lib
```

#### Ubuntu/Debian

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git libopencv-dev

# Clone with submodules
git clone --recursive https://github.com/user/OutlineTool.git
cd OutlineTool

# Build library
make lib
```

### Existing Repository Setup

If you already have the repository cloned, initialize the submodule:

```bash
git submodule update --init --recursive
make lib
```

## Building

### Quick Build (Recommended)

```bash
make build
```

### Manual CMake Build

```bash
mkdir build
cd build
cmake ..
make -j4
```

### Build Options

```bash
# Debug build with debugging symbols
make debug

# Release build (optimized)
make release

# Clean build directory
make clean

# Install to system
make install
```

### Custom Dependency Paths

If dependencies are installed in non-standard locations:

```bash
mkdir build
cd build
cmake .. \
  -DOpenCV_DIR=/path/to/opencv/lib/cmake/opencv4 \
  -DDXFRW_LIBRARY=/path/to/libdxfrw.so \
  -DDXFRW_INCLUDE_DIR=/path/to/dxfrw/headers
make
```

## Usage

### Command-Line Usage

```bash
./build/OutlineTool -i input_image.jpg -o output.dxf
```

#### Command-Line Options

- `-i, --input` - Input image file path (required)
- `-o, --output` - Output DXF file path (optional, auto-generated if not specified)

#### Examples

```bash
# Convert a single image
./build/OutlineTool -i photo.jpg -o drawing.dxf

# Auto-generate output filename (creates photo.dxf)
./build/OutlineTool -i photo.jpg

# Process multiple files
for img in *.jpg; do
  ./build/OutlineTool -i "$img"
done
```

## Library Integration

### C/C++ Integration

```c
#include <OutlineToolAPI.h>

int main() {
    OutlineToolParams params;
    outline_tool_get_default_params(&params);
    
    OutlineToolResult result = outline_tool_process_image_to_dxf(
        "input.jpg", 
        "output.dxf", 
        &params, 
        NULL, NULL
    );
    
    return (result == OUTLINE_TOOL_SUCCESS) ? 0 : 1;
}
```

Compile with: `gcc -loutlinetool myapp.c`

### Swift Package Manager Integration

Create a Swift package that wraps this library:

```swift
// Package.swift
.systemLibrary(
    name: "COutlineTool",
    pkgConfig: "outlinetool",
    providers: [.brew(["outlinetool"])]
)
```

### CMake Integration

```cmake
find_package(OutlineTool REQUIRED)
target_link_libraries(myapp OutlineTool::OutlineToolLib)
```

### pkg-config Integration

```bash
# Get compile flags
pkg-config --cflags outlinetool

# Get link flags  
pkg-config --libs outlinetool

# Use in build
gcc $(pkg-config --cflags --libs outlinetool) myapp.c
```

## How It Works

1. **Image Loading** - Loads and validates the input image
2. **Preprocessing** - Converts to grayscale and applies binary thresholding
3. **Boundary Detection** - Finds the largest contour (lightbox/document boundary)
4. **Perspective Correction** - Warps the image to a normalized square format
5. **Noise Reduction** - Applies morphological operations and Gaussian blur
6. **Object Detection** - Identifies the main object contour
7. **DXF Export** - Converts pixel coordinates to real-world measurements and saves as DXF

## Input Requirements

- **Image Format**: Any format supported by OpenCV (JPG, PNG, BMP, TIFF, etc.)
- **Minimum Size**: 100x100 pixels
- **Content**: Object photographed on a contrasting background (lightbox recommended)
- **Boundary**: Clear rectangular boundary for automatic perspective correction

## Output

- **Format**: DXF (Drawing Exchange Format)
- **Content**: Closed polyline representing the object outline
- **Units**: Millimeters (configurable in source)
- **Layer**: "Default" layer with standard properties

## Troubleshooting

### Build Issues

**OpenCV not found:**
```bash
# macOS with Homebrew
export OpenCV_DIR=/opt/homebrew/lib/cmake/opencv4

# Ubuntu
sudo apt install libopencv-dev
```

**Submodule not initialized:**
```bash
# Initialize the libdxfrw submodule
git submodule update --init --recursive
```

**CMake version errors:**
```bash
# Ensure you have CMake 3.16 or newer
cmake --version
# Update if needed via package manager
```

### Processing Issues

**"No contours found":**
- Ensure the image has good contrast between object and background
- Try adjusting lighting when taking the photo
- Make sure the object is clearly visible

**"Expected 4 corners":**
- Ensure there's a clear rectangular boundary in the image
- The lightbox or document edges should be clearly visible
- Remove any clutter from the image edges

**"Image too small":**
- Use images with at least 100x100 pixel resolution
- Higher resolution provides better accuracy

## Development

### Project Structure

```
OutlineTool/
├── src/                    # Source files
│   ├── main.cpp           # Main application
│   ├── ImageProcessor.cpp # Image processing functions
│   └── DXFWriter.cpp      # DXF export functionality
├── include/               # Header files
│   ├── ImageProcessor.hpp
│   └── DXFWriter.hpp
├── build/                 # Build directory (generated)
├── CMakeLists.txt        # CMake configuration
├── Makefile              # Build convenience wrapper
└── README.md             # This file
```

### Building from Source

1. Ensure all dependencies are installed
2. Clone the repository
3. Run `make build` or use CMake manually
4. Executable will be created as `build/OutlineTool`

## License

[Specify your license here]

## Contributing

[Specify contribution guidelines here]
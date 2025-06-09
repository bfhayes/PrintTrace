# PrintTrace

A high-performance C++ library for extracting object outlines from photos and exporting them as DXF vector files optimized for 3D printing. Designed for easy integration into other applications, with a clean C API perfect for Swift Package Manager integration.

## Features

- **High-performance C++ core** - Optimized image processing algorithms  
- **Clean C API** - Perfect for Swift, Python, and other language bindings
- **Pipeline stage control** - Process to any intermediate step for UI visualization
- **Advanced object detection** - Multi-contour merging preserves all object parts
- **Configurable thresholding** - Manual, adaptive, or Otsu with offset controls
- **Morphological control** - Disable or adjust cleaning to preserve detail
- **Automatic perspective correction** - Detects document/lightbox boundaries
- **Real-world scaling** - Accurate pixel-to-millimeter conversion with parameter ranges
- **3D printing optimization** - Smoothing and tolerance features
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
git clone --recursive https://github.com/user/PrintTrace.git
cd PrintTrace

# Build library
make lib
```

#### Ubuntu/Debian

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git libopencv-dev

# Clone with submodules
git clone --recursive https://github.com/user/PrintTrace.git
cd PrintTrace

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
./build/PrintTrace -i input_image.jpg -o output.dxf
```

#### Command-Line Options

**Basic Options:**
- `-i, --input` - Input image file path (required)
- `-o, --output` - Output DXF file path (optional, auto-generated if not specified)
- `-t, --tolerance <mm>` - Add tolerance/clearance for 3D printing (default: 0.0)
- `-s, --smooth` - Enable smoothing to remove small details
- `-d, --debug` - Save debug images showing each processing step
- `-v, --verbose` - Enable detailed output

**Object Detection Controls:**
- `--adaptive-threshold` - Use adaptive thresholding (better for uneven lighting)
- `--manual-threshold <0-255>` - Manual threshold value (0 = auto, overrides Otsu)
- `--threshold-offset <-50 to +50>` - Adjust Otsu threshold (negative = more inclusive)
- `--disable-morphology` - Disable morphological cleaning (preserves peripheral detail)
- `--morph-kernel-size <3-15>` - Size of morphological kernel (smaller = gentler cleaning)

#### Examples

```bash
# Basic outline extraction
printtrace -i photo.jpg

# 3D printing case with 1mm clearance
printtrace -i photo.jpg -t 1.0

# Smooth outline for easier printing
printtrace -i photo.jpg -s -t 0.5

# Debug mode to see processing steps
printtrace -i photo.jpg -d

# Preserve peripheral detail (like small switches, tabs, etc.)
printtrace -i photo.jpg --disable-morphology

# More inclusive thresholding to capture fine details  
printtrace -i photo.jpg --threshold-offset -15

# Use adaptive thresholding for complex lighting
printtrace -i photo.jpg --adaptive-threshold

# Gentle morphological cleaning
printtrace -i photo.jpg --morph-kernel-size 3

# Manual threshold control
printtrace -i photo.jpg --manual-threshold 120
```

## Library Integration

### C/C++ Integration

```c
#include <PrintTraceAPI.h>

int main() {
    PrintTraceParams params;
    print_trace_get_default_params(&params);
    
    // Basic processing
    PrintTraceResult result = print_trace_process_image_to_dxf(
        "input.jpg", 
        "output.dxf", 
        &params, 
        NULL, NULL, NULL
    );
    
    return (result == PRINT_TRACE_SUCCESS) ? 0 : 1;
}
```

**Advanced Features:**

```c
// Process to specific pipeline stage for UI visualization
PrintTraceImageData stage_image;
PrintTraceContour contour;

print_trace_process_to_stage(
    "input.jpg", &params, 
    PRINT_TRACE_STAGE_LIGHTBOX_CROPPED,  // Get perspective-corrected image
    &stage_image, &contour,
    NULL, NULL, NULL
);

// Clean up
print_trace_free_image_data(&stage_image);
print_trace_free_contour(&contour);
```

**Parameter Configuration:**

```c
// Get parameter ranges for UI sliders
PrintTraceParamRanges ranges;
print_trace_get_param_ranges(&ranges);

// Configure object detection
params.threshold_offset = -15.0;        // More inclusive thresholding
params.disable_morphology = true;       // Preserve peripheral detail
params.merge_nearby_contours = true;    // Handle multi-part objects
params.contour_merge_distance_mm = 5.0; // Merge parts within 5mm
```

Compile with: `gcc -lprinttrace myapp.c`

### Swift Package Manager Integration

Create a Swift package that wraps this library:

```swift
// Package.swift
.systemLibrary(
    name: "CPrintTrace",
    pkgConfig: "printtrace",
    providers: [.brew(["printtrace"])]
)
```

### CMake Integration

```cmake
find_package(PrintTrace REQUIRED)
target_link_libraries(myapp PrintTrace::PrintTraceLib)
```

### pkg-config Integration

```bash
# Get compile flags
pkg-config --cflags printtrace

# Get link flags  
pkg-config --libs printtrace

# Use in build
gcc $(pkg-config --cflags --libs printtrace) myapp.c
```

## How It Works

PrintTrace converts photos of objects into precise DXF outlines perfect for 3D printing and CAD applications.

📖 **[See detailed processing pipeline documentation](docs/README.md)**

### Quick Overview
1. **Lightbox Detection** - Finds the white background square
2. **Perspective Correction** - Removes camera angle distortion  
3. **Object Detection** - Extracts the dark object from white background
4. **3D Printing Optimization** - Optional smoothing and tolerance features
5. **DXF Export** - Saves as real-world scaled vector file

### Key Features for 3D Printing
- **Smoothing** (`-s`) - Removes small details that cause print artifacts
- **Tolerance** (`-t 1.0`) - Adds clearance for proper part fitting
- **Real-world accuracy** - Calibrated millimeter measurements

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
PrintTrace/
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
4. Executable will be created as `build/PrintTrace`

## License

[Specify your license here]

## Contributing

[Specify contribution guidelines here]
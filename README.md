# OutlineTool

A C++ application that processes images to extract object outlines and exports them as DXF vector files. Perfect for converting photographed objects into CAD-compatible vector formats.

## Features

- **Automatic perspective correction** - Detects document/lightbox boundaries and corrects perspective
- **Noise reduction** - Advanced image processing to clean up contours
- **Real-world scaling** - Converts pixel coordinates to accurate millimeter measurements
- **DXF export** - Creates industry-standard DXF files compatible with CAD software
- **Command-line interface** - Simple and efficient batch processing

## Prerequisites

### Required Dependencies

- **CMake** (>= 3.16)
- **C++ Compiler** with C++17 support (GCC, Clang, or MSVC)
- **OpenCV** (>= 4.0) with core, imgproc, and imgcodecs modules
- **libdxfrw** - DXF reading/writing library

### Quick Installation (Recommended)

Run the automated dependency installer:

```bash
./install-deps.sh
```

This script will:
- Install OpenCV and build tools via your package manager
- Build and install libdxfrw from source
- Configure everything automatically

### Manual Installation

#### macOS (with Homebrew)

```bash
# Install dependencies
brew install cmake opencv git

# Build and install libdxfrw from source
git clone https://github.com/aewallin/libdxfrw.git
cd libdxfrw
mkdir build && cd build
cmake .. && make
sudo make install
cd ../..

# Build OutlineTool
make build
```

#### Ubuntu/Debian

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake git libopencv-dev

# Build and install libdxfrw from source
git clone https://github.com/aewallin/libdxfrw.git
cd libdxfrw
mkdir build && cd build
cmake .. && make
sudo make install
sudo ldconfig
cd ../..

# Build OutlineTool
make build
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

### Swift/iOS/macOS Integration

OutlineTool provides a C API that can be easily integrated into Swift applications:

```bash
# Build shared library for Swift integration
make dylib
```

This creates `build/liboutlinetool.dylib` which can be used in Xcode projects.

#### Swift API Example

```swift
import SwiftUI

struct ContentView: View {
    @StateObject private var outlineTool = OutlineTool()
    
    var body: some View {
        VStack {
            if outlineTool.isProcessing {
                ProgressView(value: outlineTool.progress)
                Text(outlineTool.currentStage)
            }
            
            Button("Process Image") {
                Task {
                    try await outlineTool.processImageToDXF(
                        imagePath: "/path/to/image.jpg",
                        outputPath: "/path/to/output.dxf"
                    )
                }
            }
        }
    }
}
```

See [`swift-example/`](swift-example/) for complete SwiftUI integration examples.

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

**libdxfrw not found:**
```bash
# Build from source if not available in package manager
git clone https://github.com/aewallin/libdxfrw
cd libdxfrw
mkdir build && cd build
cmake .. && make
sudo make install
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
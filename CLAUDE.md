# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Quick Build (Recommended)
```bash
make build
```

### Alternative Build Methods
```bash
# Debug build
make debug

# Release build
make release

# Manual CMake
mkdir build && cd build && cmake .. && make
```

The executable will be created as `build/OutlineTool`.

## Architecture Overview

This is a clean, modular C++ application that processes images to extract object outlines and exports them as DXF vector files. The codebase is organized into separate modules for maintainability.

### Project Structure

```
src/
├── main.cpp              # Command-line interface and orchestration
├── ImageProcessor.cpp    # All image processing functionality
└── DXFWriter.cpp        # DXF file export functionality

include/
├── ImageProcessor.hpp    # Image processing class interface
└── DXFWriter.hpp        # DXF writer class interface
```

### Key Components

1. **ImageProcessor Class** (`include/ImageProcessor.hpp`, `src/ImageProcessor.cpp`):
   - Static methods for each processing step
   - Configurable parameters via ProcessingParams struct
   - Complete image-to-contour pipeline in `processImageToContour()`
   - Individual functions: loading, thresholding, perspective correction, noise removal

2. **DXFWriter Class** (`include/DXFWriter.hpp`, `src/DXFWriter.cpp`):
   - Implements libdxfrw's DRW_Interface
   - Static `saveContourAsDXF()` method for simple usage
   - Handles coordinate conversion from pixels to millimeters
   - Creates closed polylines for CAD compatibility

3. **Main Application** (`src/main.cpp`):
   - Clean command-line argument parsing
   - Comprehensive error handling with specific error types
   - Input validation (file existence, image size)
   - Simple orchestration of processing pipeline

### Processing Pipeline

1. Load and validate image (`ImageProcessor::loadImage()`)
2. Convert to grayscale and threshold (`convertToGrayscale()`, `thresholdImage()`)
3. Find document/lightbox boundary (`findLargestContour()`, `approximatePolygon()`)
4. Apply perspective correction (`warpImage()`)
5. Clean up noise (`removeNoise()`)
6. Extract object contour (`findMainContour()`)
7. Export to DXF (`DXFWriter::saveContourAsDXF()`)

### Dependencies

- **OpenCV** (core, imgproc, imgcodecs): Image processing operations
- **libdxfrw**: DXF file reading/writing library
- **CMake** (>=3.16): Build system with automatic dependency detection

### Configuration

Default processing parameters are defined in `ImageProcessor::ProcessingParams`:
- `warpSize`: Target image size after perspective correction (3240px)
- `realWorldSizeMM`: Real-world size corresponding to warpSize (162mm)  
- `thresholdValue`: Binary threshold value (127)
- Noise reduction parameters (kernel sizes, blur amount)

### Usage Pattern

```bash
./build/OutlineTool -i input.jpg [-o output.dxf]
```

The tool expects images with clear rectangular boundaries (lightbox/document) and objects on contrasting backgrounds.
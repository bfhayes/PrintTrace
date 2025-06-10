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

The executable will be created as `build/PrintTrace`.

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

1. Load and validate image (`ImageProcessor::loadImage()`) - **Keep color**
2. Normalize lighting on color image (`normalizeLighting()` with Lab CLAHE)
3. Detect paper boundary using color analysis (`detectEdges()` with Lab color space)
4. Find boundary contour and refine corners (`findBoundaryContour()`, `refineCorners()`)
5. Apply perspective correction on color image (`warpImage()`)
6. **Convert to grayscale for object detection** (`convertToGrayscale()`)
7. Extract object contour within paper (`findObjectContour()`)
8. Export to DXF (`DXFWriter::saveContourAsDXF()`)

### Dependencies

- **OpenCV** (core, imgproc, imgcodecs): Image processing operations
- **libdxfrw**: DXF file reading/writing library
- **CMake** (>=3.16): Build system with automatic dependency detection

### Configuration

Processing parameters are defined in `PrintTraceParams` with full range validation:

**Lightbox Parameters (Now supports rectangles!):**
- `lightbox_width_mm`: Real-world lightbox width in millimeters (range: 10.0-500.0, default: 162.0)
- `lightbox_height_mm`: Real-world lightbox height in millimeters (range: 10.0-500.0, default: 162.0)
- `lightbox_width_px`: Auto-calculated from MM dimensions and pixels-per-mm ratio
- `lightbox_height_px`: Auto-calculated from MM dimensions and pixels-per-mm ratio
- **Pixels per MM ratio**: Default 10.0, adjustable via `--pixels-per-mm`

**Object Detection Parameters:**
- `use_adaptive_threshold`: Use adaptive thresholding instead of Otsu (default: false)
- `manual_threshold`: Manual threshold value (range: 0-255, 0 = auto, default: 0)
- `threshold_offset`: Offset from auto threshold (range: -50.0 to +50.0, default: 0)

**Morphological Processing:**
- `disable_morphology`: Disable morphological cleaning to preserve detail (default: false)
- `morph_kernel_size`: Morphological kernel size (range: 3-15, default: 5)

**Multi-Contour Detection:**
- `merge_nearby_contours`: Merge multiple contours into one object (default: true)
- `contour_merge_distance_mm`: Max distance to merge contours (range: 1.0-20.0, default: 5.0)

**3D Printing Optimization:**
- `dilation_amount_mm`: Amount to dilate outline for clearance (range: 0.0-10.0, default: 0.0)
- `enable_smoothing`: Enable smoothing for easier 3D printing (default: false)
- `smoothing_amount_mm`: Smoothing amount (range: 0.1-2.0, default: 0.2)

**Performance Optimization:**
- `enable_inpainting`: Enable inpainting for cleaner paper isolation (default: false)
  - When disabled: Uses simple masking (faster)
  - When enabled: Uses cv::inpaint to remove objects from paper (slower but cleaner)

### Usage Pattern

```bash
./build/PrintTrace -i input.jpg [-o output.dxf] [options]
```

**New CLI Options for Advanced Control:**
```bash
# Lightbox setup (pixels auto-calculated from MM dimensions)
--lightbox-width-mm <mm>               # Real-world lightbox width (default: 162.0)
--lightbox-height-mm <mm>              # Real-world lightbox height (default: 162.0)
--pixels-per-mm <ratio>                # Pixels per millimeter ratio (default: 10.0)

# Object detection controls
--adaptive-threshold                    # Better for uneven lighting
--manual-threshold <0-255>             # Override automatic threshold
--threshold-offset <-50 to +50>        # Adjust auto threshold (negative = more inclusive)

# Morphological processing controls  
--disable-morphology                   # Preserve peripheral detail (switches, tabs, etc.)
--morph-kernel-size <3-15>            # Gentler cleaning (3 = gentle, 15 = aggressive)

# Performance controls
--enable-inpainting                    # Better paper isolation but slower (off by default)

# Debug and analysis
-d, --debug                           # Save step-by-step debug images
-v, --verbose                         # Detailed processing information
```

**Pipeline Stage Control API:**
```c
// Process to any intermediate stage for UI visualization
print_trace_process_to_stage(input, params, PRINT_TRACE_STAGE_LIGHTBOX_CROPPED, &image, &contour, ...);

// Available stages:
// PRINT_TRACE_STAGE_LOADED              - Image loaded and grayscaled
// PRINT_TRACE_STAGE_LIGHTBOX_CROPPED    - Perspective corrected (uniform dimensions)
// PRINT_TRACE_STAGE_NORMALIZED          - Lighting normalized
// PRINT_TRACE_STAGE_BOUNDARY_DETECTED   - Lightbox boundary found
// PRINT_TRACE_STAGE_OBJECT_DETECTED     - Object contour extracted  
// PRINT_TRACE_STAGE_SMOOTHED            - Smoothed for 3D printing
// PRINT_TRACE_STAGE_DILATED             - Dilated for tolerance
// PRINT_TRACE_STAGE_FINAL               - Final validated contour
```

The tool expects images with clear rectangular boundaries (lightbox/document) and objects on contrasting backgrounds.

**Performance Optimizations:**
- **Inpainting disabled by default** - Significantly faster processing without cv::inpaint
- **Optimized median calculation** - Uses mean approximation instead of full pixel sorting
- **Configurable Canny thresholds** - Skip auto-calculation when thresholds are provided
- **Default pixels-per-mm reduced to 10** - Smaller default image sizes (1620×1620 instead of 3240×3240)
- **Delayed grayscale conversion** - Keeps color data until step 6 for better paper boundary detection

**Key Improvements:**
- **Multi-contour detection** now preserves small object parts (like switches, tabs)
- **Parameter ranges** available via `print_trace_get_param_ranges()` for UI sliders
- **Stage-by-stage processing** enables real-time UI feedback
- **Enhanced threshold controls** handle difficult lighting conditions
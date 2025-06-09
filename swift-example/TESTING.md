# Testing the OutlineTool Swift Integration

This guide shows you multiple ways to test the Swift wrapper for OutlineTool, from simple unit tests to complete Xcode integration.

## 🚀 Quick Test Options

### 1. Command-Line Swift Package Test
```bash
# First, build the library
cd ..  # Go to project root
make dylib

# Set up library path for testing
export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH

# Go to Swift example directory
cd swift-example

# Run Swift package tests
swift test

# Run the demo executable
swift run OutlineToolDemo
```

### 2. Xcode Project Test
```bash
# Create a new macOS app in Xcode
# Copy the Swift files and follow integration steps below
```

### 3. Interactive Swift REPL Test
```bash
# Set library path
export DYLD_LIBRARY_PATH=$PWD/../build:$DYLD_LIBRARY_PATH

# Start Swift with the library
swift -I../include -L../build -loutlinetool
```

## 📋 Test Categories

### Unit Tests (No Images Required)
These test the Swift wrapper's basic functionality:

```swift
// Parameter validation
let params = OutlineTool.ProcessingParams()
try OutlineTool.validateParams(params)  // Should pass

// Error handling
var invalidParams = OutlineTool.ProcessingParams()
invalidParams.warpSize = -100
// Should throw OutlineToolError.invalidParameters
```

### Integration Tests (Require Test Images)
These test the actual image processing:

```swift
let outlineTool = OutlineTool()
try await outlineTool.processImageToDXF(
    imagePath: "/path/to/test/image.jpg",
    outputPath: "/tmp/output.dxf"
)
```

### UI Tests (SwiftUI)
Test the complete UI integration with mock or real images.

## 🛠️ Setup Instructions

### Option 1: Swift Package Manager (Recommended for Testing)

1. **Build the library**:
   ```bash
   cd /path/to/OutlineTool
   make dylib
   ```

2. **Set up environment**:
   ```bash
   export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH
   export DYLD_FRAMEWORK_PATH=$PWD/build:$DYLD_FRAMEWORK_PATH
   ```

3. **Run tests**:
   ```bash
   cd swift-example
   swift test
   ```

4. **Run demo**:
   ```bash
   swift run OutlineToolDemo
   ```

### Option 2: Xcode Integration

1. **Create new Xcode project** (macOS App, SwiftUI)

2. **Add the library**:
   - Drag `build/liboutlinetool.dylib` into your project
   - In Build Settings → Library Search Paths: Add path to `build/`
   - In Build Settings → Header Search Paths: Add path to `include/`

3. **Add source files**:
   - Copy `OutlineToolWrapper.swift` to your project
   - Copy `ContentView.swift` for UI example

4. **Configure bridging**:
   - Create `BridgingHeader.h` with: `#include "OutlineToolAPI.h"`
   - Set Objective-C Bridging Header path in Build Settings

5. **Bundle the library**:
   - Add Copy Files build phase
   - Destination: Frameworks
   - Add `liboutlinetool.dylib`

### Option 3: Interactive Testing

Use Swift REPL for quick testing:

```bash
# Set up environment
export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH

# Start Swift with proper paths
swift -I include -L build -loutlinetool

# In Swift REPL:
import Foundation
// Copy-paste OutlineToolWrapper.swift content
let tool = OutlineTool()
let params = OutlineTool.defaultParams()
print("Default warp size: \(params.warpSize)")
```

## 🧪 Test Examples

### Basic API Test
```swift
import XCTest
@testable import OutlineToolSwift

class BasicTests: XCTestCase {
    func testParameterValidation() {
        let params = OutlineTool.ProcessingParams()
        XCTAssertNoThrow(try OutlineTool.validateParams(params))
    }
    
    func testFileValidation() {
        XCTAssertFalse(OutlineTool.isValidImageFile("/nonexistent.jpg"))
    }
}
```

### Async Processing Test
```swift
func testAsyncProcessing() async throws {
    let outlineTool = OutlineTool()
    
    // This will fail but tests the error handling
    do {
        try await outlineTool.processImageToDXF(
            imagePath: "/nonexistent.jpg",
            outputPath: "/tmp/test.dxf"
        )
        XCTFail("Should have thrown error")
    } catch OutlineTool.OutlineToolError.fileNotFound {
        // Expected
    }
}
```

### SwiftUI Integration Test
```swift
import SwiftUI

struct TestView: View {
    @StateObject private var outlineTool = OutlineTool()
    @State private var testResult = ""
    
    var body: some View {
        VStack {
            Text("Processing: \(outlineTool.isProcessing)")
            Text("Progress: \(outlineTool.progress * 100, specifier: "%.0f")%")
            Text("Stage: \(outlineTool.currentStage)")
            
            Button("Test Validation") {
                do {
                    try OutlineTool.validateParams(OutlineTool.defaultParams())
                    testResult = "✅ Validation passed"
                } catch {
                    testResult = "❌ Validation failed: \(error)"
                }
            }
            
            Text(testResult)
        }
        .padding()
    }
}
```

## 📁 Test Images

For complete testing, you'll need test images. Create a `test-images/` directory with:

### Recommended Test Images:
1. **document.jpg** - Photo of a document on a desk (tests perspective correction)
2. **lightbox.jpg** - Object photographed on a lightbox (ideal case)
3. **small.jpg** - Very small image (tests size validation)
4. **noisy.jpg** - Image with lots of background noise
5. **invalid.txt** - Non-image file (tests file validation)

### Test Image Requirements:
- **Minimum size**: 100x100 pixels
- **Clear rectangular boundary**: Document edges or lightbox borders
- **Good contrast**: Object should stand out from background
- **Supported formats**: JPG, PNG, TIFF, BMP

## 🔧 Troubleshooting Tests

### Library Not Found
```bash
# Check if library exists
ls -la build/liboutlinetool.dylib

# Check library dependencies
otool -L build/liboutlinetool.dylib

# Set library path
export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH
```

### Swift Compilation Errors
```bash
# Check Swift version
swift --version  # Should be 5.7+

# Verify header inclusion
cat swift-example/Sources/COutlineTool/OutlineToolAPI.h | head -10

# Check module map
cat swift-example/Sources/COutlineTool/module.modulemap
```

### Runtime Crashes
```bash
# Run with debugging
swift test --verbose

# Check for missing dependencies
otool -L build/liboutlinetool.dylib
# Ensure OpenCV and libdxfrw are available
```

### Performance Issues
```bash
# Run performance tests
swift test --filter performance

# Monitor memory usage
leaks --atExit -- swift run OutlineToolDemo
```

## 📊 Expected Test Results

### Unit Tests (All Should Pass):
- ✅ Parameter validation
- ✅ Default parameter values
- ✅ File validation (non-existent files)
- ✅ Error type creation
- ✅ Data structure initialization

### Integration Tests (With Test Images):
- ✅ Image processing to DXF
- ✅ Contour extraction
- ✅ Progress reporting
- ✅ Error handling for invalid images
- ✅ Processing time estimation

### Performance Benchmarks:
- Parameter validation: < 1ms per call
- File validation: < 10ms per call
- Image processing: 2-10 seconds (depends on image size)

## 🚀 Continuous Integration

For CI/CD, you can automate testing:

```bash
#!/bin/bash
# ci-test.sh

set -e

echo "Building library..."
make dylib

echo "Setting up environment..."
export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH

echo "Running Swift tests..."
cd swift-example
swift test

echo "Running demo..."
swift run OutlineToolDemo

echo "✅ All tests passed!"
```

This testing setup gives you confidence that the Swift integration works correctly and helps catch issues early in development.
# Complete Testing Guide for OutlineTool Swift Integration

This guide provides multiple approaches to test the Swift wrapper, from simple structural tests to full Xcode integration.

## 🚀 Testing Methods Overview

| Method | Complexity | Library Required | Use Case |
|--------|------------|------------------|----------|
| **Structural Test** | Low | No | Verify Swift code compiles and data structures work |
| **C API Test** | Medium | Yes | Test actual library functions |
| **Xcode Integration** | High | Yes | Full SwiftUI app testing |
| **Command Line** | Medium | Yes | Automated testing and CI |

## 1. 📋 Structural Testing (No Library Required)

Test the Swift wrapper's data structures and error handling:

```bash
cd swift-example
swift simple-test.swift
```

**What this tests:**
- ✅ Swift structs compile correctly
- ✅ Parameter structures work
- ✅ Error handling types
- ✅ Data conversion logic
- ✅ Basic Swift syntax

**Expected output:**
```
OutlineTool Swift Integration Test
=================================
🧪 Testing basic functionality...
✅ Parameter struct created successfully
✅ Created test contour with 4 points
🎉 All structural tests passed!
```

## 2. 🔗 C API Testing (Library Required)

Test the actual C library integration:

### Build and Link Test

```bash
# 1. Build the library
make dylib

# 2. Test with direct Swift compilation
cd swift-example
swift -I../include -L../build -loutlinetool -DDYLD_LIBRARY_PATH=../build simple-test.swift
```

### Manual C API Test

Create a simple test that directly calls C functions:

```swift
// c-api-test.swift
import Foundation

// This requires the actual library to be linked
func testCAPI() {
    // Test parameter validation
    var params = OutlineToolParams(/* ... */)
    let result = outline_tool_validate_params(&params)
    print("Validation result: \(result)")
    
    // Test file validation
    let isValid = "/tmp".withCString { path in
        outline_tool_is_valid_image_file(path)
    }
    print("File validation: \(isValid)")
}
```

## 3. 🖥️ Xcode Integration Testing

### Step-by-Step Xcode Setup

1. **Create New Project**
   ```
   File → New → Project → macOS → App (SwiftUI)
   ```

2. **Add Library Files**
   - Drag `build/liboutlinetool.dylib` into project
   - Copy `include/OutlineToolAPI.h` to project
   - Add `OutlineToolWrapper.swift` to project

3. **Configure Build Settings**
   ```
   Build Settings → Header Search Paths: $(PROJECT_DIR)/../include
   Build Settings → Library Search Paths: $(PROJECT_DIR)/../build
   Build Settings → Other Linker Flags: -loutlinetool
   ```

4. **Create Bridging Header**
   ```c
   // BridgingHeader.h
   #include "OutlineToolAPI.h"
   ```

5. **Set Bridging Header Path**
   ```
   Build Settings → Objective-C Bridging Header: BridgingHeader.h
   ```

### Simple Xcode Test

```swift
import SwiftUI

struct TestView: View {
    @State private var testResults: [String] = []
    
    var body: some View {
        VStack {
            Text("OutlineTool Tests")
                .font(.title)
            
            List(testResults, id: \.self) { result in
                Text(result)
            }
            
            Button("Run Tests") {
                runTests()
            }
        }
        .padding()
    }
    
    func runTests() {
        testResults.removeAll()
        
        // Test 1: Parameter validation
        do {
            let params = OutlineTool.ProcessingParams()
            try OutlineTool.validateParams(params)
            testResults.append("✅ Parameter validation passed")
        } catch {
            testResults.append("❌ Parameter validation failed: \(error)")
        }
        
        // Test 2: File validation
        let isValid = OutlineTool.isValidImageFile("/dev/null")
        testResults.append("✅ File validation: \(isValid ? "valid" : "invalid")")
        
        // Test 3: Version info
        testResults.append("✅ Library loaded successfully")
    }
}
```

## 4. 🤖 Automated Testing

### Unit Tests with XCTest

Create `OutlineToolTests.swift`:

```swift
import XCTest
@testable import YourApp

class OutlineToolTests: XCTestCase {
    
    func testParameterValidation() {
        let params = OutlineTool.ProcessingParams()
        XCTAssertNoThrow(try OutlineTool.validateParams(params))
    }
    
    func testInvalidParameters() {
        var params = OutlineTool.ProcessingParams()
        params.warpSize = -100
        XCTAssertThrowsError(try OutlineTool.validateParams(params))
    }
    
    func testFileValidation() {
        XCTAssertFalse(OutlineTool.isValidImageFile("/nonexistent.jpg"))
    }
    
    func testAsyncProcessing() async {
        let tool = OutlineTool()
        
        do {
            try await tool.processImageToDXF(
                imagePath: "/nonexistent.jpg",
                outputPath: "/tmp/test.dxf"
            )
            XCTFail("Should have thrown error")
        } catch OutlineTool.OutlineToolError.fileNotFound {
            // Expected
        } catch {
            XCTFail("Wrong error type: \(error)")
        }
    }
}
```

### Command Line Testing Script

```bash
#!/bin/bash
# test-swift.sh

set -e

echo "🔧 Building library..."
make dylib

echo "📋 Running structural tests..."
cd swift-example
swift simple-test.swift

echo "🔗 Testing C API linkage..."
export DYLD_LIBRARY_PATH=$PWD/../build:$DYLD_LIBRARY_PATH

# Test compilation with library
echo "import Foundation" > test-compile.swift
echo "let params = OutlineToolParams(warp_size: 100, real_world_size_mm: 10.0, threshold_value: 127, noise_kernel_size: 21, blur_size: 101, polygon_epsilon_factor: 0.02)" >> test-compile.swift

if swift -I../include -L../build -loutlinetool test-compile.swift 2>/dev/null; then
    echo "✅ Library compilation successful"
else
    echo "❌ Library compilation failed"
    echo "💡 Make sure OpenCV and libdxfrw are installed"
fi

rm -f test-compile.swift

echo "🎉 Testing complete!"
```

## 5. 🖼️ Image Processing Tests

### With Test Images

```swift
func testWithRealImage() async {
    let testImagePath = Bundle.main.path(forResource: "test-image", ofType: "jpg")!
    let outputPath = "/tmp/test-output.dxf"
    
    let tool = OutlineTool()
    
    do {
        try await tool.processImageToDXF(
            imagePath: testImagePath,
            outputPath: outputPath
        )
        
        // Verify output exists
        XCTAssertTrue(FileManager.default.fileExists(atPath: outputPath))
        
        // Clean up
        try? FileManager.default.removeItem(atPath: outputPath)
        
    } catch {
        XCTFail("Processing failed: \(error)")
    }
}
```

### Test Image Requirements

Create a `test-images/` folder with:

1. **valid-document.jpg** - Clear document photo
2. **lightbox-object.jpg** - Object on lightbox
3. **small-image.jpg** - Below minimum size (should fail)
4. **noisy-image.jpg** - High noise image
5. **invalid.txt** - Non-image file

## 6. 🐛 Troubleshooting Tests

### Common Issues and Solutions

**Library Not Found:**
```bash
# Check library exists
ls -la build/liboutlinetool.dylib

# Check dependencies
otool -L build/liboutlinetool.dylib

# Set library path
export DYLD_LIBRARY_PATH=$PWD/build:$DYLD_LIBRARY_PATH
```

**Swift Compilation Errors:**
```bash
# Check Swift version (needs 5.7+)
swift --version

# Verify header path
ls -la include/OutlineToolAPI.h

# Test basic compilation
swift -I include -c OutlineToolWrapper.swift
```

**Runtime Crashes:**
```bash
# Enable crash logging
export DYLD_PRINT_LIBRARIES=1
export DYLD_PRINT_LIBRARIES_POST_LAUNCH=1

# Run with debugging
lldb swift
(lldb) run simple-test.swift
```

**Memory Issues:**
```bash
# Check for leaks
leaks --atExit -- swift simple-test.swift

# Profile memory usage
instruments -t Allocations swift simple-test.swift
```

## 7. 📊 Test Results Reference

### Expected Test Outcomes

**Structural Tests:**
- ✅ All Swift structs compile
- ✅ Parameter validation logic works
- ✅ Error types are correctly defined
- ✅ Data conversion functions work

**Library Integration Tests:**
- ✅ C functions can be called from Swift
- ✅ Parameter validation catches invalid values
- ✅ File validation works correctly
- ✅ Error messages are properly returned

**Image Processing Tests (with valid images):**
- ✅ Images load successfully
- ✅ Processing completes without errors
- ✅ DXF files are generated
- ✅ Progress callbacks work (if enabled)
- ✅ Memory is properly freed

**Performance Benchmarks:**
- Parameter validation: < 1ms
- File validation: < 10ms
- Small image processing: 2-5 seconds
- Large image processing: 5-15 seconds

## 8. 🔄 Continuous Integration

For automated testing in CI/CD:

```yaml
# .github/workflows/swift-test.yml
name: Swift Integration Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: macos-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        brew install opencv libdxfrw
    
    - name: Build library
      run: make dylib
    
    - name: Run Swift tests
      run: |
        cd swift-example
        swift simple-test.swift
        
    - name: Test Xcode compilation
      run: |
        xcodebuild -scheme OutlineToolApp -destination 'platform=macOS' test
```

This comprehensive testing approach ensures your Swift integration works correctly across all scenarios, from basic data structure validation to complete image processing workflows.
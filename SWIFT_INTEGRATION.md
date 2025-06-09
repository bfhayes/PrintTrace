# Swift Package Manager Integration Guide

This guide shows how to create a Swift Package that wraps the PrintTrace C++ library.

## Overview

The PrintTrace library is designed to be easily integrated into Swift packages using the system library approach. This allows Swift developers to use the high-performance C++ image processing capabilities with a type-safe Swift API.

## Integration Architecture

```
Swift Package
├── Package.swift              # Package definition
├── Sources/
│   ├── CPrintTrace/          # System library wrapper
│   │   ├── module.modulemap   # C module definition
│   │   └── shim.h            # Header imports
│   └── PrintTrace/          # Swift wrapper
│       └── PrintTrace.swift # Type-safe Swift API
└── Tests/
    └── PrintTraceTests/     # Swift tests
```

## Step 1: Install PrintTrace Library

First, clone and install the PrintTrace library system-wide:

```bash
# Clone with submodules
git clone --recursive https://github.com/user/PrintTrace.git
cd PrintTrace

# Build and install the library
make install-lib

# Verify installation
pkg-config --exists printtrace && echo "✅ PrintTrace found"
pkg-config --cflags --libs printtrace
```

## Step 2: Create Swift Package

Create a new Swift package:

```bash
mkdir SwiftPrintTrace
cd SwiftPrintTrace
swift package init --type library
```

## Step 3: Configure Package.swift

Update your `Package.swift`:

```swift
// swift-tools-version: 5.7
import PackageDescription

let package = Package(
    name: "SwiftPrintTrace",
    platforms: [
        .macOS(.v10_15),
        .iOS(.v13)
    ],
    products: [
        .library(
            name: "SwiftPrintTrace",
            targets: ["SwiftPrintTrace"]
        )
    ],
    targets: [
        // System library target for PrintTrace
        .systemLibrary(
            name: "CPrintTrace",
            pkgConfig: "printtrace",
            providers: [
                .brew(["printtrace"]),
                .apt(["libprinttrace-dev"])
            ]
        ),
        
        // Swift wrapper target
        .target(
            name: "SwiftPrintTrace",
            dependencies: ["CPrintTrace"]
        ),
        
        // Tests
        .testTarget(
            name: "SwiftPrintTraceTests",
            dependencies: ["SwiftPrintTrace"]
        )
    ]
)
```

## Step 4: Create System Library Module

Create `Sources/CPrintTrace/module.modulemap`:

```c
module CPrintTrace {
    header "shim.h"
    link "printtrace"
    export *
}
```

Create `Sources/CPrintTrace/shim.h`:

```c
#ifndef CPRINTTRACE_SHIM_H
#define CPRINTTRACE_SHIM_H

#include <PrintTraceAPI.h>

#endif /* CPRINTTRACE_SHIM_H */
```

## Step 5: Create Swift Wrapper

Create `Sources/SwiftPrintTrace/PrintTrace.swift`:

```swift
import Foundation
import CPrintTrace

@available(macOS 10.15, iOS 13.0, *)
public class PrintTrace: ObservableObject {
    
    public struct ProcessingParams {
        public var warpSize: Int32 = 3240
        public var realWorldSizeMM: Double = 162.0
        public var thresholdValue: Int32 = 127
        public var noiseKernelSize: Int32 = 21
        public var blurSize: Int32 = 101
        public var polygonEpsilonFactor: Double = 0.02
        
        public init() {}
        
        internal func toCStruct() -> PrintTraceParams {
            return PrintTraceParams(
                warp_size: warpSize,
                real_world_size_mm: realWorldSizeMM,
                threshold_value: thresholdValue,
                noise_kernel_size: noiseKernelSize,
                blur_size: blurSize,
                polygon_epsilon_factor: polygonEpsilonFactor
            )
        }
    }
    
    public enum PrintTraceError: Error, LocalizedError {
        case invalidInput(String)
        case fileNotFound(String)
        case processingFailed(String)
        
        public var errorDescription: String? {
            switch self {
            case .invalidInput(let msg): return "Invalid input: \(msg)"
            case .fileNotFound(let msg): return "File not found: \(msg)"
            case .processingFailed(let msg): return "Processing failed: \(msg)"
            }
        }
    }
    
    @Published public var isProcessing: Bool = false
    @Published public var progress: Double = 0.0
    
    public init() {}
    
    public static func validateParams(_ params: ProcessingParams) throws {
        var cParams = params.toCStruct()
        let result = print_trace_validate_params(&cParams)
        
        if result != PRINT_TRACE_SUCCESS {
            let message = String(cString: print_trace_get_error_message(result))
            throw PrintTraceError.processingFailed(message)
        }
    }
    
    public static func isValidImageFile(_ path: String) -> Bool {
        return path.withCString { cPath in
            return print_trace_is_valid_image_file(cPath)
        }
    }
    
    public func processImageToDXF(
        imagePath: String,
        outputPath: String,
        params: ProcessingParams = ProcessingParams()
    ) async throws {
        
        await MainActor.run {
            isProcessing = true
            progress = 0.0
        }
        
        defer {
            Task { @MainActor in
                isProcessing = false
            }
        }
        
        return try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos: .userInitiated).async {
                var cParams = params.toCStruct()
                
                let result = imagePath.withCString { cImagePath in
                    return outputPath.withCString { cOutputPath in
                        return print_trace_process_image_to_dxf(
                            cImagePath,
                            cOutputPath,
                            &cParams,
                            nil, // No progress callback for now
                            nil  // No error callback for now
                        )
                    }
                }
                
                if result == PRINT_TRACE_SUCCESS {
                    continuation.resume()
                } else {
                    let message = String(cString: print_trace_get_error_message(result))
                    continuation.resume(throwing: PrintTraceError.processingFailed(message))
                }
            }
        }
    }
}
```

## Step 6: Create Tests

Create `Tests/SwiftPrintTraceTests/SwiftPrintTraceTests.swift`:

```swift
import XCTest
@testable import SwiftPrintTrace

final class SwiftPrintTraceTests: XCTestCase {
    
    func testParameterValidation() throws {
        let params = PrintTrace.ProcessingParams()
        XCTAssertNoThrow(try PrintTrace.validateParams(params))
    }
    
    func testFileValidation() {
        XCTAssertFalse(PrintTrace.isValidImageFile("/nonexistent.jpg"))
    }
    
    func testInvalidParameters() {
        var params = PrintTrace.ProcessingParams()
        params.warpSize = -100
        XCTAssertThrowsError(try PrintTrace.validateParams(params))
    }
}
```

## Step 7: Build and Test

```bash
# Build the Swift package
swift build

# Run tests
swift test

# Test in Xcode
open Package.swift
```

## Step 8: Use in Other Projects

Once your Swift package is ready, others can use it:

```swift
// In their Package.swift
.package(url: "https://github.com/user/SwiftPrintTrace", from: "1.0.0")

// In their Swift code
import SwiftPrintTrace

let tool = PrintTrace()
try await tool.processImageToDXF(
    imagePath: "input.jpg",
    outputPath: "output.dxf"
)
```

## Distribution

### Option 1: Homebrew Formula

Create a Homebrew formula for easy installation:

```ruby
class Printtrace < Formula
  desc "Image outline extraction and DXF conversion library"
  homepage "https://github.com/user/PrintTrace"
  url "https://github.com/user/PrintTrace/archive/v1.0.0.tar.gz"
  
  depends_on "cmake" => :build
  depends_on "opencv"
  depends_on "libdxfrw"
  
  def install
    system "make", "install-lib", "PREFIX=#{prefix}"
  end
end
```

### Option 2: Direct Source Distribution

Users can build and install directly:

```bash
git clone --recursive https://github.com/user/PrintTrace
cd PrintTrace
make install-lib
```

## Key Benefits

✅ **Type Safety** - Swift wrapper provides compile-time safety  
✅ **Performance** - Direct C++ performance with no overhead  
✅ **Integration** - Works seamlessly with SwiftUI and Combine  
✅ **Distribution** - Easy to distribute via Swift Package Manager  
✅ **Maintenance** - C++ library can be updated independently  
✅ **Testing** - Full test coverage for both C API and Swift wrapper  

This approach gives you the best of both worlds: high-performance C++ processing with a delightful Swift developer experience.
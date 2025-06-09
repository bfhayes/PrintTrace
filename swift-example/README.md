# OutlineTool Swift Integration Example

This directory contains a complete SwiftUI example showing how to integrate the OutlineTool C library into iOS/macOS applications.

## Files Overview

| File | Description |
|------|-------------|
| `OutlineToolWrapper.swift` | Swift wrapper providing a type-safe interface to the C API |
| `ContentView.swift` | SwiftUI example app with full UI for image processing |
| `BridgingHeader.h` | Objective-C bridging header for importing C API |
| `README.md` | This documentation file |

## Integration Steps

### 1. Build the Shared Library

First, build the OutlineTool shared library:

```bash
cd ..  # Go back to project root
make dylib
```

This creates `build/liboutlinetool.dylib` which you'll link to your Xcode project.

### 2. Create Xcode Project

1. Create a new SwiftUI project in Xcode
2. Add the following files to your project:
   - `OutlineToolWrapper.swift`
   - `ContentView.swift` (or integrate into your existing UI)
   - `BridgingHeader.h`

### 3. Configure Xcode Project

#### Add the Library
1. Drag `build/liboutlinetool.dylib` into your Xcode project
2. In Build Settings → Other Linker Flags, add: `-loutlinetool`
3. In Build Settings → Library Search Paths, add the path to the dylib

#### Set Up Bridging Header
1. In Build Settings → Objective-C Bridging Header, set path to `BridgingHeader.h`
2. In Build Settings → Header Search Paths, add path to `include/` directory

#### Copy Headers
Copy `include/OutlineToolAPI.h` to your Xcode project or ensure it's in the header search path.

### 4. Bundle the Library (macOS/iOS)

For distribution, you need to bundle the library with your app:

#### macOS
1. Add a "Copy Files" build phase
2. Set destination to "Frameworks"
3. Add `liboutlinetool.dylib`

#### iOS
For iOS, you need to build the library for iOS architectures (arm64, x86_64 for simulator).

## Swift API Usage

### Basic Usage

```swift
import SwiftUI

struct MyView: View {
    @StateObject private var outlineTool = OutlineTool()
    
    var body: some View {
        VStack {
            // Processing with progress updates
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

### Advanced Usage

```swift
// Custom processing parameters
var params = OutlineTool.ProcessingParams()
params.warpSize = 4000
params.thresholdValue = 150
params.realWorldSizeMM = 200.0

// Process with custom parameters
try await outlineTool.processImageToDXF(
    imagePath: inputPath,
    outputPath: outputPath,
    params: params
)

// Extract contour data for custom processing
let contour = try await outlineTool.processImageToContour(
    imagePath: inputPath,
    params: params
)

// Access individual points
for point in contour.points {
    print("Point: (\(point.x), \(point.y)) mm")
}
```

### Error Handling

```swift
do {
    try await outlineTool.processImageToDXF(
        imagePath: imagePath,
        outputPath: outputPath
    )
} catch OutlineTool.OutlineToolError.imageLoadFailed(let message) {
    print("Failed to load image: \(message)")
} catch OutlineTool.OutlineToolError.noContours(let message) {
    print("No contours found: \(message)")
} catch {
    print("Unexpected error: \(error)")
}
```

## SwiftUI Integration Features

### ObservableObject Support
The `OutlineTool` class is an `ObservableObject` with `@Published` properties:

- `isProcessing: Bool` - Whether processing is currently running
- `progress: Double` - Processing progress (0.0 to 1.0)
- `currentStage: String` - Current processing stage description
- `lastError: OutlineToolError?` - Last error that occurred

### Async/Await Support
All processing methods are async and work seamlessly with SwiftUI:

```swift
Button("Process") {
    Task {
        await processImage()
    }
}
```

### File System Integration
Built-in support for SwiftUI file importers and exporters:

```swift
.fileImporter(
    isPresented: $showingImagePicker,
    allowedContentTypes: [.image],
    allowsMultipleSelection: false
) { result in
    // Handle image selection
}
```

## Library Dependencies

The shared library includes these dependencies:
- OpenCV (image processing)
- libdxfrw (DXF file generation)

Make sure these are available on the target system or bundle them with your app.

## Platform Support

- **macOS**: 10.15+ (for basic features), 12.0+ (for full SwiftUI example)
- **iOS**: 13.0+ (for basic features), 15.0+ (for full SwiftUI example)
- **Architecture**: Universal (x86_64, arm64)

## Performance Considerations

- Image processing runs on a background queue to avoid blocking the UI
- Progress updates are dispatched to the main queue for UI updates
- Large images may take several seconds to process
- Memory usage scales with image size

## Troubleshooting

### Library Not Found
Ensure the dylib is in the correct path and linked properly:
```bash
otool -L YourApp.app/Contents/MacOS/YourApp
```

### Missing Headers
Verify the bridging header path and header search paths in Build Settings.

### Runtime Crashes
Check that all dependencies (OpenCV, libdxfrw) are available on the target system.

### iOS Deployment
For iOS deployment, you need to build the library for iOS architectures. Consider using a cross-compilation setup or building on macOS for iOS targets.
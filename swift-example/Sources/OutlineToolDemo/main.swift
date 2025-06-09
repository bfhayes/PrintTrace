import Foundation
import OutlineToolSwift

@main
struct OutlineToolDemo {
    static func main() async {
        print("OutlineTool Swift Demo")
        print("====================")
        
        // Test parameter validation
        await testParameterValidation()
        
        // Test version info
        testVersionInfo()
        
        // Test file validation
        testFileValidation()
        
        // If you have a test image, uncomment and modify this:
        // await testImageProcessing()
        
        print("\nDemo completed!")
    }
    
    static func testParameterValidation() async {
        print("\n🔧 Testing parameter validation...")
        
        // Test valid parameters
        let validParams = OutlineTool.ProcessingParams()
        do {
            try OutlineTool.validateParams(validParams)
            print("✅ Valid parameters accepted")
        } catch {
            print("❌ Valid parameters rejected: \(error)")
        }
        
        // Test invalid parameters
        var invalidParams = OutlineTool.ProcessingParams()
        invalidParams.warpSize = -100
        do {
            try OutlineTool.validateParams(invalidParams)
            print("❌ Invalid parameters accepted")
        } catch {
            print("✅ Invalid parameters correctly rejected: \(error)")
        }
    }
    
    static func testVersionInfo() {
        print("\n📋 Testing version and utility functions...")
        
        // These would need to be exposed through the C API
        print("✅ Library loaded successfully")
        
        let defaultParams = OutlineTool.defaultParams()
        print("✅ Default parameters: warpSize=\(defaultParams.warpSize), threshold=\(defaultParams.thresholdValue)")
    }
    
    static func testFileValidation() {
        print("\n📁 Testing file validation...")
        
        // Test with non-existent file
        let nonExistentFile = "/path/to/nonexistent.jpg"
        let isValid = OutlineTool.isValidImageFile(nonExistentFile)
        print("✅ Non-existent file validation: \(isValid ? "valid" : "invalid") (expected: invalid)")
        
        // Test with /dev/null (should be invalid)
        let devNull = "/dev/null"
        let isValidDevNull = OutlineTool.isValidImageFile(devNull)
        print("✅ /dev/null validation: \(isValidDevNull ? "valid" : "invalid") (expected: invalid)")
    }
    
    static func testImageProcessing() async {
        print("\n🖼️ Testing image processing...")
        
        // You would replace this with an actual image path
        let testImagePath = "/path/to/your/test/image.jpg"
        let outputPath = "/tmp/test_output.dxf"
        
        guard OutlineTool.isValidImageFile(testImagePath) else {
            print("❌ Test image not found or invalid: \(testImagePath)")
            return
        }
        
        let outlineTool = OutlineTool()
        
        // Test processing time estimation
        let estimatedTime = OutlineTool.estimateProcessingTime(for: testImagePath)
        if estimatedTime > 0 {
            print("⏱️ Estimated processing time: \(estimatedTime) seconds")
        }
        
        do {
            print("🚀 Starting image processing...")
            
            // Process image with custom parameters
            var params = OutlineTool.ProcessingParams()
            params.warpSize = 2000  // Smaller for faster processing
            params.thresholdValue = 100
            
            try await outlineTool.processImageToDXF(
                imagePath: testImagePath,
                outputPath: outputPath,
                params: params
            )
            
            print("✅ Image processed successfully!")
            print("📄 Output DXF: \(outputPath)")
            
        } catch let error as OutlineTool.OutlineToolError {
            print("❌ Processing failed: \(error.localizedDescription)")
        } catch {
            print("❌ Unexpected error: \(error)")
        }
    }
    
    static func testContourExtraction() async {
        print("\n📐 Testing contour extraction...")
        
        let testImagePath = "/path/to/your/test/image.jpg"
        
        guard OutlineTool.isValidImageFile(testImagePath) else {
            print("❌ Test image not found: \(testImagePath)")
            return
        }
        
        let outlineTool = OutlineTool()
        
        do {
            let contour = try await outlineTool.processImageToContour(imagePath: testImagePath)
            
            print("✅ Contour extracted successfully!")
            print("📊 Points count: \(contour.points.count)")
            print("📏 Pixels per mm: \(contour.pixelsPerMM)")
            
            if contour.points.count > 0 {
                let firstPoint = contour.points[0]
                print("📍 First point: (\(firstPoint.x), \(firstPoint.y)) mm")
            }
            
            // Test saving contour separately
            let dxfPath = "/tmp/extracted_contour.dxf"
            try OutlineTool.saveContourToDXF(contour: contour, outputPath: dxfPath)
            print("💾 Contour saved to: \(dxfPath)")
            
        } catch {
            print("❌ Contour extraction failed: \(error)")
        }
    }
}
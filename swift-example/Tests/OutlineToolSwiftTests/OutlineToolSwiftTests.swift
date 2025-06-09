import XCTest
@testable import OutlineToolSwift

@available(macOS 10.15, iOS 13.0, *)
final class OutlineToolSwiftTests: XCTestCase {
    
    func testParameterValidation() throws {
        // Test valid parameters
        let validParams = OutlineTool.ProcessingParams()
        XCTAssertNoThrow(try OutlineTool.validateParams(validParams))
        
        // Test invalid warp size
        var invalidParams = OutlineTool.ProcessingParams()
        invalidParams.warpSize = -100
        XCTAssertThrowsError(try OutlineTool.validateParams(invalidParams)) { error in
            XCTAssertTrue(error is OutlineTool.OutlineToolError)
        }
        
        // Test invalid threshold
        invalidParams = OutlineTool.ProcessingParams()
        invalidParams.thresholdValue = 300
        XCTAssertThrowsError(try OutlineTool.validateParams(invalidParams))
        
        // Test invalid kernel size (even number)
        invalidParams = OutlineTool.ProcessingParams()
        invalidParams.noiseKernelSize = 20
        XCTAssertThrowsError(try OutlineTool.validateParams(invalidParams))
    }
    
    func testDefaultParameters() {
        let params = OutlineTool.defaultParams()
        
        XCTAssertEqual(params.warpSize, 3240)
        XCTAssertEqual(params.realWorldSizeMM, 162.0, accuracy: 0.01)
        XCTAssertEqual(params.thresholdValue, 127)
        XCTAssertEqual(params.noiseKernelSize, 21)
        XCTAssertEqual(params.blurSize, 101)
        XCTAssertEqual(params.polygonEpsilonFactor, 0.02, accuracy: 0.001)
    }
    
    func testParameterConversion() {
        let swiftParams = OutlineTool.ProcessingParams(
            warpSize: 4000,
            realWorldSizeMM: 200.0,
            thresholdValue: 150
        )
        
        let cParams = swiftParams.toCStruct()
        
        XCTAssertEqual(cParams.warp_size, 4000)
        XCTAssertEqual(cParams.real_world_size_mm, 200.0, accuracy: 0.01)
        XCTAssertEqual(cParams.threshold_value, 150)
    }
    
    func testFileValidation() {
        // Test with non-existent file
        XCTAssertFalse(OutlineTool.isValidImageFile("/path/to/nonexistent.jpg"))
        
        // Test with invalid file
        XCTAssertFalse(OutlineTool.isValidImageFile("/dev/null"))
        
        // Test with directory
        XCTAssertFalse(OutlineTool.isValidImageFile("/tmp"))
    }
    
    func testProcessingTimeEstimation() {
        // Test with non-existent file
        let time = OutlineTool.estimateProcessingTime(for: "/path/to/nonexistent.jpg")
        XCTAssertEqual(time, -1.0)
    }
    
    func testPointCreation() {
        let point = OutlineTool.Point(x: 10.5, y: 20.3)
        XCTAssertEqual(point.x, 10.5, accuracy: 0.01)
        XCTAssertEqual(point.y, 20.3, accuracy: 0.01)
    }
    
    func testContourCreation() {
        let points = [
            OutlineTool.Point(x: 0, y: 0),
            OutlineTool.Point(x: 10, y: 0),
            OutlineTool.Point(x: 10, y: 10),
            OutlineTool.Point(x: 0, y: 10)
        ]
        
        let contour = OutlineTool.Contour(points: points, pixelsPerMM: 20.0)
        
        XCTAssertEqual(contour.points.count, 4)
        XCTAssertEqual(contour.pixelsPerMM, 20.0, accuracy: 0.01)
        XCTAssertEqual(contour.points[0].x, 0, accuracy: 0.01)
        XCTAssertEqual(contour.points[2].y, 10, accuracy: 0.01)
    }
    
    func testErrorTypes() {
        let error1 = OutlineTool.OutlineToolError.fileNotFound("test.jpg")
        XCTAssertNotNil(error1.localizedDescription)
        XCTAssertTrue(error1.localizedDescription!.contains("File not found"))
        
        let error2 = OutlineTool.OutlineToolError.invalidParameters("Invalid threshold")
        XCTAssertNotNil(error2.localizedDescription)
        XCTAssertTrue(error2.localizedDescription!.contains("Invalid parameters"))
    }
    
    func testObservableObjectProperties() {
        let outlineTool = OutlineTool()
        
        XCTAssertFalse(outlineTool.isProcessing)
        XCTAssertEqual(outlineTool.progress, 0.0, accuracy: 0.01)
        XCTAssertEqual(outlineTool.currentStage, "")
        XCTAssertNil(outlineTool.lastError)
    }
    
    // MARK: - Integration Tests (require actual image files)
    
    func testImageProcessingWithMockFile() async throws {
        // This test would require a real image file
        // For now, we test that the error handling works correctly
        
        let outlineTool = OutlineTool()
        
        do {
            try await outlineTool.processImageToDXF(
                imagePath: "/path/to/nonexistent.jpg",
                outputPath: "/tmp/output.dxf"
            )
            XCTFail("Should have thrown an error for non-existent file")
        } catch OutlineTool.OutlineToolError.fileNotFound {
            // Expected behavior
        } catch {
            XCTFail("Unexpected error type: \(error)")
        }
    }
    
    func testContourExtractionWithMockFile() async throws {
        let outlineTool = OutlineTool()
        
        do {
            let _ = try await outlineTool.processImageToContour(
                imagePath: "/path/to/nonexistent.jpg"
            )
            XCTFail("Should have thrown an error for non-existent file")
        } catch OutlineTool.OutlineToolError.fileNotFound {
            // Expected behavior
        } catch {
            XCTFail("Unexpected error type: \(error)")
        }
    }
    
    // MARK: - Performance Tests
    
    func testParameterValidationPerformance() throws {
        let params = OutlineTool.ProcessingParams()
        
        measure {
            for _ in 0..<1000 {
                try? OutlineTool.validateParams(params)
            }
        }
    }
    
    func testFileValidationPerformance() {
        measure {
            for _ in 0..<100 {
                _ = OutlineTool.isValidImageFile("/dev/null")
            }
        }
    }
}

// MARK: - Mock Tests with Real Images (Optional)

@available(macOS 10.15, iOS 13.0, *)
extension OutlineToolSwiftTests {
    
    /// Test with a real image file (you need to provide the path)
    func disabled_testRealImageProcessing() async throws {
        let testImagePath = "/path/to/your/test/image.jpg"
        let outputPath = "/tmp/test_output.dxf"
        
        // Skip if test image doesn't exist
        guard OutlineTool.isValidImageFile(testImagePath) else {
            throw XCTSkip("Test image not available at \(testImagePath)")
        }
        
        let outlineTool = OutlineTool()
        
        // Test processing time estimation
        let estimatedTime = OutlineTool.estimateProcessingTime(for: testImagePath)
        XCTAssertGreaterThan(estimatedTime, 0)
        
        // Test actual processing
        try await outlineTool.processImageToDXF(
            imagePath: testImagePath,
            outputPath: outputPath
        )
        
        // Verify output file exists
        XCTAssertTrue(FileManager.default.fileExists(atPath: outputPath))
        
        // Clean up
        try? FileManager.default.removeItem(atPath: outputPath)
    }
    
    /// Test contour extraction with real image
    func disabled_testRealContourExtraction() async throws {
        let testImagePath = "/path/to/your/test/image.jpg"
        
        guard OutlineTool.isValidImageFile(testImagePath) else {
            throw XCTSkip("Test image not available")
        }
        
        let outlineTool = OutlineTool()
        let contour = try await outlineTool.processImageToContour(imagePath: testImagePath)
        
        XCTAssertGreaterThan(contour.points.count, 0)
        XCTAssertGreaterThan(contour.pixelsPerMM, 0)
        
        // Test saving extracted contour
        let dxfPath = "/tmp/extracted_contour.dxf"
        try OutlineTool.saveContourToDXF(contour: contour, outputPath: dxfPath)
        
        XCTAssertTrue(FileManager.default.fileExists(atPath: dxfPath))
        
        // Clean up
        try? FileManager.default.removeItem(atPath: dxfPath)
    }
}
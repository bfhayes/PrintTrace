import Foundation

// MARK: - OutlineTool Swift Wrapper

/// Swift wrapper for the OutlineTool C library
/// Provides a type-safe, Swift-friendly interface for image processing
@available(macOS 10.15, iOS 13.0, *)
public class OutlineTool: ObservableObject {
    
    // MARK: - Types
    
    /// Processing parameters for image processing
    public struct ProcessingParams {
        public var warpSize: Int32 = 3240
        public var realWorldSizeMM: Double = 162.0
        public var thresholdValue: Int32 = 127
        public var noiseKernelSize: Int32 = 21
        public var blurSize: Int32 = 101
        public var polygonEpsilonFactor: Double = 0.02
        
        public init() {}
        
        public init(warpSize: Int32 = 3240,
                   realWorldSizeMM: Double = 162.0,
                   thresholdValue: Int32 = 127,
                   noiseKernelSize: Int32 = 21,
                   blurSize: Int32 = 101,
                   polygonEpsilonFactor: Double = 0.02) {
            self.warpSize = warpSize
            self.realWorldSizeMM = realWorldSizeMM
            self.thresholdValue = thresholdValue
            self.noiseKernelSize = noiseKernelSize
            self.blurSize = blurSize
            self.polygonEpsilonFactor = polygonEpsilonFactor
        }
        
        /// Convert to C struct
        internal func toCStruct() -> OutlineToolParams {
            return OutlineToolParams(
                warp_size: warpSize,
                real_world_size_mm: realWorldSizeMM,
                threshold_value: thresholdValue,
                noise_kernel_size: noiseKernelSize,
                blur_size: blurSize,
                polygon_epsilon_factor: polygonEpsilonFactor
            )
        }
    }
    
    /// Point in 2D space (millimeters)
    public struct Point {
        public let x: Double
        public let y: Double
        
        public init(x: Double, y: Double) {
            self.x = x
            self.y = y
        }
    }
    
    /// Contour data with points and scaling information
    public struct Contour {
        public let points: [Point]
        public let pixelsPerMM: Double
        
        public init(points: [Point], pixelsPerMM: Double) {
            self.points = points
            self.pixelsPerMM = pixelsPerMM
        }
    }
    
    /// OutlineTool errors
    public enum OutlineToolError: Error, LocalizedError {
        case invalidInput(String)
        case fileNotFound(String)
        case imageLoadFailed(String)
        case imageTooSmall(String)
        case noContours(String)
        case noBoundary(String)
        case noObject(String)
        case dxfWriteFailed(String)
        case invalidParameters(String)
        case processingFailed(String)
        case unknown(String)
        
        public var errorDescription: String? {
            switch self {
            case .invalidInput(let msg): return "Invalid input: \(msg)"
            case .fileNotFound(let msg): return "File not found: \(msg)"
            case .imageLoadFailed(let msg): return "Image load failed: \(msg)"
            case .imageTooSmall(let msg): return "Image too small: \(msg)"
            case .noContours(let msg): return "No contours: \(msg)"
            case .noBoundary(let msg): return "No boundary: \(msg)"
            case .noObject(let msg): return "No object: \(msg)"
            case .dxfWriteFailed(let msg): return "DXF write failed: \(msg)"
            case .invalidParameters(let msg): return "Invalid parameters: \(msg)"
            case .processingFailed(let msg): return "Processing failed: \(msg)"
            case .unknown(let msg): return "Unknown error: \(msg)"
            }
        }
    }
    
    // MARK: - Published Properties for SwiftUI
    
    @Published public var isProcessing: Bool = false
    @Published public var progress: Double = 0.0
    @Published public var currentStage: String = ""
    @Published public var lastError: OutlineToolError?
    
    // MARK: - Initialization
    
    public init() {}
    
    // MARK: - Public API
    
    /// Get default processing parameters
    public static func defaultParams() -> ProcessingParams {
        return ProcessingParams()
    }
    
    /// Validate processing parameters
    public static func validateParams(_ params: ProcessingParams) throws {
        var cParams = params.toCStruct()
        let result = outline_tool_validate_params(&cParams)
        
        if result != OUTLINE_TOOL_SUCCESS {
            let message = String(cString: outline_tool_get_error_message(result))
            throw convertError(result, message: message)
        }
    }
    
    /// Check if file is a valid image
    public static func isValidImageFile(_ path: String) -> Bool {
        return path.withCString { cPath in
            return outline_tool_is_valid_image_file(cPath)
        }
    }
    
    /// Estimate processing time for image
    public static func estimateProcessingTime(for imagePath: String) -> TimeInterval {
        return imagePath.withCString { cPath in
            let time = outline_tool_estimate_processing_time(cPath)
            return time >= 0 ? TimeInterval(time) : -1
        }
    }
    
    /// Process image to extract contour (async for SwiftUI)
    @MainActor
    public func processImageToContour(
        imagePath: String,
        params: ProcessingParams = ProcessingParams()
    ) async throws -> Contour {
        
        isProcessing = true
        progress = 0.0
        currentStage = "Initializing"
        lastError = nil
        
        defer {
            isProcessing = false
        }
        
        return try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos: .userInitiated).async {
                do {
                    let contour = try self.processImageToContourSync(
                        imagePath: imagePath,
                        params: params
                    )
                    continuation.resume(returning: contour)
                } catch {
                    DispatchQueue.main.async {
                        self.lastError = error as? OutlineToolError
                    }
                    continuation.resume(throwing: error)
                }
            }
        }
    }
    
    /// Process image to DXF file (async for SwiftUI)
    @MainActor
    public func processImageToDXF(
        imagePath: String,
        outputPath: String,
        params: ProcessingParams = ProcessingParams()
    ) async throws {
        
        isProcessing = true
        progress = 0.0
        currentStage = "Initializing"
        lastError = nil
        
        defer {
            isProcessing = false
        }
        
        return try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.global(qos: .userInitiated).async {
                do {
                    try self.processImageToDXFSync(
                        imagePath: imagePath,
                        outputPath: outputPath,
                        params: params
                    )
                    continuation.resume()
                } catch {
                    DispatchQueue.main.async {
                        self.lastError = error as? OutlineToolError
                    }
                    continuation.resume(throwing: error)
                }
            }
        }
    }
    
    /// Save contour to DXF file
    public static func saveContourToDXF(contour: Contour, outputPath: String) throws {
        // Convert Swift contour to C contour
        let pointsArray = contour.points.map { point in
            OutlineToolPoint(x: point.x, y: point.y)
        }
        
        try pointsArray.withUnsafeBufferPointer { buffer in
            var cContour = OutlineToolContour(
                points: UnsafeMutablePointer(mutating: buffer.baseAddress),
                point_count: Int32(buffer.count),
                pixels_per_mm: contour.pixelsPerMM
            )
            
            let result = outputPath.withCString { cOutputPath in
                return outline_tool_save_contour_to_dxf(&cContour, cOutputPath, nil)
            }
            
            if result != OUTLINE_TOOL_SUCCESS {
                let message = String(cString: outline_tool_get_error_message(result))
                throw convertError(result, message: message)
            }
        }
    }
    
    // MARK: - Private Methods
    
    private func processImageToContourSync(
        imagePath: String,
        params: ProcessingParams
    ) throws -> Contour {
        
        var cParams = params.toCStruct()
        var cContour = OutlineToolContour(points: nil, point_count: 0, pixels_per_mm: 0.0)
        
        // Set up progress callback
        let progressCallback: OutlineToolProgressCallback = { [weak self] progress, stage in
            DispatchQueue.main.async {
                self?.progress = progress
                self?.currentStage = String(cString: stage!)
            }
        }
        
        // Set up error callback
        let errorCallback: OutlineToolErrorCallback = { _, _ in
            // Error handling is done through return value
        }
        
        let result = imagePath.withCString { cImagePath in
            return outline_tool_process_image_to_contour(
                cImagePath,
                &cParams,
                &cContour,
                progressCallback,
                errorCallback
            )
        }
        
        defer {
            outline_tool_free_contour(&cContour)
        }
        
        if result != OUTLINE_TOOL_SUCCESS {
            let message = String(cString: outline_tool_get_error_message(result))
            throw Self.convertError(result, message: message)
        }
        
        // Convert C contour to Swift contour
        let points = (0..<cContour.point_count).map { i in
            let cPoint = cContour.points[Int(i)]
            return Point(x: cPoint.x, y: cPoint.y)
        }
        
        return Contour(points: points, pixelsPerMM: cContour.pixels_per_mm)
    }
    
    private func processImageToDXFSync(
        imagePath: String,
        outputPath: String,
        params: ProcessingParams
    ) throws {
        
        var cParams = params.toCStruct()
        
        // Set up progress callback
        let progressCallback: OutlineToolProgressCallback = { [weak self] progress, stage in
            DispatchQueue.main.async {
                self?.progress = progress
                self?.currentStage = String(cString: stage!)
            }
        }
        
        // Set up error callback
        let errorCallback: OutlineToolErrorCallback = { _, _ in
            // Error handling is done through return value
        }
        
        let result = imagePath.withCString { cImagePath in
            return outputPath.withCString { cOutputPath in
                return outline_tool_process_image_to_dxf(
                    cImagePath,
                    cOutputPath,
                    &cParams,
                    progressCallback,
                    errorCallback
                )
            }
        }
        
        if result != OUTLINE_TOOL_SUCCESS {
            let message = String(cString: outline_tool_get_error_message(result))
            throw Self.convertError(result, message: message)
        }
    }
    
    private static func convertError(_ result: OutlineToolResult, message: String) -> OutlineToolError {
        switch result {
        case OUTLINE_TOOL_ERROR_INVALID_INPUT:
            return .invalidInput(message)
        case OUTLINE_TOOL_ERROR_FILE_NOT_FOUND:
            return .fileNotFound(message)
        case OUTLINE_TOOL_ERROR_IMAGE_LOAD_FAILED:
            return .imageLoadFailed(message)
        case OUTLINE_TOOL_ERROR_IMAGE_TOO_SMALL:
            return .imageTooSmall(message)
        case OUTLINE_TOOL_ERROR_NO_CONTOURS:
            return .noContours(message)
        case OUTLINE_TOOL_ERROR_NO_BOUNDARY:
            return .noBoundary(message)
        case OUTLINE_TOOL_ERROR_NO_OBJECT:
            return .noObject(message)
        case OUTLINE_TOOL_ERROR_DXF_WRITE_FAILED:
            return .dxfWriteFailed(message)
        case OUTLINE_TOOL_ERROR_INVALID_PARAMETERS:
            return .invalidParameters(message)
        case OUTLINE_TOOL_ERROR_PROCESSING_FAILED:
            return .processingFailed(message)
        default:
            return .unknown(message)
        }
    }
}

// MARK: - SwiftUI Integration Helpers

@available(macOS 10.15, iOS 13.0, *)
extension OutlineTool {
    
    /// Convenient method for SwiftUI file import
    public func processImageFromURL(
        _ url: URL,
        outputURL: URL,
        params: ProcessingParams = ProcessingParams()
    ) async throws {
        
        // Ensure we have file access
        let accessing = url.startAccessingSecurityScopedResource()
        defer {
            if accessing {
                url.stopAccessingSecurityScopedResource()
            }
        }
        
        try await processImageToDXF(
            imagePath: url.path,
            outputPath: outputURL.path,
            params: params
        )
    }
}
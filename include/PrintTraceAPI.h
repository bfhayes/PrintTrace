#ifndef PRINT_TRACE_API_H
#define PRINT_TRACE_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Version information
#define PRINT_TRACE_VERSION_MAJOR 1
#define PRINT_TRACE_VERSION_MINOR 0
#define PRINT_TRACE_VERSION_PATCH 0

// Error codes for Swift integration
typedef enum {
    PRINT_TRACE_SUCCESS = 0,
    PRINT_TRACE_ERROR_INVALID_INPUT = -1,
    PRINT_TRACE_ERROR_FILE_NOT_FOUND = -2,
    PRINT_TRACE_ERROR_IMAGE_LOAD_FAILED = -3,
    PRINT_TRACE_ERROR_IMAGE_TOO_SMALL = -4,
    PRINT_TRACE_ERROR_NO_CONTOURS = -5,
    PRINT_TRACE_ERROR_NO_BOUNDARY = -6,
    PRINT_TRACE_ERROR_NO_OBJECT = -7,
    PRINT_TRACE_ERROR_DXF_WRITE_FAILED = -8,
    PRINT_TRACE_ERROR_INVALID_PARAMETERS = -9,
    PRINT_TRACE_ERROR_PROCESSING_FAILED = -10
} PrintTraceResult;

// Processing parameters structure (CAD-optimized)
typedef struct {
    // Lightbox dimensions after perspective correction
    int32_t lightbox_width_px;      // Target lightbox width in pixels (range: 500-8000, default: 3240)
    int32_t lightbox_height_px;     // Target lightbox height in pixels (range: 500-8000, default: 3240)
    double lightbox_width_mm;       // Real-world lightbox width in millimeters (range: 10.0-500.0, default: 162.0)
    double lightbox_height_mm;      // Real-world lightbox height in millimeters (range: 10.0-500.0, default: 162.0)
    
    // Edge detection parameters (replacing threshold)
    double canny_lower;             // Canny lower threshold (range: 10.0-200.0, default: 50.0)
    double canny_upper;             // Canny upper threshold (range: 50.0-400.0, default: 150.0)
    int32_t canny_aperture;         // Canny aperture size (range: 3,5,7, default: 3)
    
    // CLAHE parameters for lighting normalization  
    double clahe_clip_limit;        // CLAHE clip limit (range: 0.5-8.0, default: 2.0)
    int32_t clahe_tile_size;        // CLAHE tile grid size (range: 4-16, default: 8)
    
    // Object detection parameters
    bool use_adaptive_threshold;    // Use adaptive thresholding instead of Otsu (default: false)
    double manual_threshold;        // Manual threshold value (range: 0-255, 0 = auto, default: 0)
    double threshold_offset;        // Offset from auto threshold (range: -50.0 to +50.0, default: 0)
    
    // Morphological processing parameters (these can remove peripheral detail)
    bool disable_morphology;        // Disable morphological cleaning (default: false)
    int32_t morph_kernel_size;      // Morphological kernel size (range: 3-15, default: 5)
    
    // Multi-contour detection parameters
    bool merge_nearby_contours;     // Merge multiple contours into one object (default: true)
    double contour_merge_distance_mm; // Max distance to merge contours in mm (range: 1.0-20.0, default: 5.0)
    
    // Contour filtering parameters
    double min_contour_area;        // Minimum contour area (range: 100.0-10000.0, default: 1000.0)
    double min_solidity;            // Minimum solidity ratio (range: 0.1-1.0, default: 0.5)
    double max_aspect_ratio;        // Maximum aspect ratio (range: 2.0-30.0, default: 10.0)
    
    // Polygon approximation (higher precision for CAD)
    double polygon_epsilon_factor;  // Polygon approximation factor (range: 0.001-0.02, default: 0.005)
    
    // Sub-pixel refinement
    bool enable_subpixel_refinement; // Enable sub-pixel accuracy (default: true)
    int32_t corner_win_size;        // Corner refinement window size (range: 3-15, default: 5)
    
    // Validation parameters
    bool validate_closed_contour;   // Validate contour closure (default: true)
    double min_perimeter;           // Minimum contour perimeter (range: 50.0-2000.0, default: 100.0)
    
    // Tolerance/dilation for 3D printing cases
    double dilation_amount_mm;      // Amount to dilate outline in millimeters (range: 0.0-10.0, default: 0.0)
    
    // Smoothing for 3D printing
    bool enable_smoothing;          // Enable smoothing for easier 3D printing (default: false)
    double smoothing_amount_mm;     // Smoothing amount in millimeters (range: 0.1-2.0, default: 0.2)
    int32_t smoothing_mode;         // Smoothing algorithm: 0=morphological, 1=curvature-based (default: 1)
    
    // Debug visualization
    bool enable_debug_output;       // Enable debug image output (default: false)
} PrintTraceParams;

// Parameter ranges structure for UI slider configuration
typedef struct {
    // Lightbox dimension ranges
    int32_t lightbox_width_px_min;  // 500
    int32_t lightbox_width_px_max;  // 8000
    int32_t lightbox_height_px_min; // 500
    int32_t lightbox_height_px_max; // 8000
    double lightbox_width_mm_min;   // 10.0
    double lightbox_width_mm_max;   // 500.0
    double lightbox_height_mm_min;  // 10.0
    double lightbox_height_mm_max;  // 500.0
    
    // Edge detection ranges
    double canny_lower_min;         // 10.0
    double canny_lower_max;         // 200.0
    double canny_upper_min;         // 50.0
    double canny_upper_max;         // 400.0
    int32_t canny_aperture_values[3]; // {3, 5, 7}
    
    // CLAHE ranges
    double clahe_clip_limit_min;    // 0.5
    double clahe_clip_limit_max;    // 8.0
    int32_t clahe_tile_size_min;    // 4
    int32_t clahe_tile_size_max;    // 16
    
    // Object detection ranges
    double manual_threshold_min;    // 0 (0 = auto)
    double manual_threshold_max;    // 255
    double threshold_offset_min;    // -50.0
    double threshold_offset_max;    // 50.0
    int32_t morph_kernel_size_min;  // 3
    int32_t morph_kernel_size_max;  // 15
    double contour_merge_distance_mm_min; // 1.0
    double contour_merge_distance_mm_max; // 20.0
    
    // Contour filtering ranges
    double min_contour_area_min;    // 100.0
    double min_contour_area_max;    // 10000.0
    double min_solidity_min;        // 0.1
    double min_solidity_max;        // 1.0
    double max_aspect_ratio_min;    // 2.0
    double max_aspect_ratio_max;    // 30.0
    
    // Polygon approximation ranges
    double polygon_epsilon_factor_min; // 0.001
    double polygon_epsilon_factor_max; // 0.02
    
    // Sub-pixel refinement ranges
    int32_t corner_win_size_min;    // 3
    int32_t corner_win_size_max;    // 15
    
    // Validation ranges
    double min_perimeter_min;       // 50.0
    double min_perimeter_max;       // 2000.0
    
    // 3D printing ranges
    double dilation_amount_mm_min;  // 0.0
    double dilation_amount_mm_max;  // 10.0
    double smoothing_amount_mm_min; // 0.1
    double smoothing_amount_mm_max; // 2.0
    int32_t smoothing_mode_min;     // 0 (morphological)
    int32_t smoothing_mode_max;     // 1 (curvature-based)
} PrintTraceParamRanges;

// Point structure for contour data
typedef struct {
    double x;
    double y;
} PrintTracePoint;

// Contour data structure
typedef struct {
    PrintTracePoint* points;
    int32_t point_count;
    double pixels_per_mm;
} PrintTraceContour;

// Image data structure for Swift integration
typedef struct {
    uint8_t* data;              // Raw image data (RGBA8888 format for Swift compatibility)
    int32_t width;              // Image width in pixels
    int32_t height;             // Image height in pixels
    int32_t channels;           // Number of channels (3 for RGB, 4 for RGBA)
    int32_t bytes_per_row;      // Number of bytes per row (including padding)
} PrintTraceImageData;

// Processing pipeline stages
typedef enum {
    PRINT_TRACE_STAGE_LOADED = 0,            // Image loaded and converted to grayscale
    PRINT_TRACE_STAGE_LIGHTBOX_CROPPED = 1,  // Perspective corrected to lightbox area (first uniform stage)
    PRINT_TRACE_STAGE_NORMALIZED = 2,        // After CLAHE normalization
    PRINT_TRACE_STAGE_BOUNDARY_DETECTED = 3, // Lightbox boundary found
    PRINT_TRACE_STAGE_OBJECT_DETECTED = 4,   // Object contour found in warped image
    PRINT_TRACE_STAGE_SMOOTHED = 5,          // After optional smoothing
    PRINT_TRACE_STAGE_DILATED = 6,           // After optional dilation
    PRINT_TRACE_STAGE_FINAL = 7,             // Final validated contour
    PRINT_TRACE_STAGE_COUNT = 8              // Total number of processing stages
} PrintTraceProcessingStage;


// Progress callback function type for SwiftUI progress tracking
typedef void (*PrintTraceProgressCallback)(double progress, const char* stage, void* user_data);

// Error callback function type for detailed error reporting
typedef void (*PrintTraceErrorCallback)(PrintTraceResult error_code, const char* error_message, void* user_data);

// Core API Functions

/**
 * Get default processing parameters
 * @param params Pointer to parameters structure to fill
 */
void print_trace_get_default_params(PrintTraceParams* params);

/**
 * Get parameter ranges for UI slider configuration
 * @param ranges Pointer to ranges structure to fill
 */
void print_trace_get_param_ranges(PrintTraceParamRanges* ranges);

/**
 * Validate processing parameters
 * @param params Pointer to parameters to validate
 * @return PRINT_TRACE_SUCCESS if valid, error code otherwise
 */
PrintTraceResult print_trace_validate_params(const PrintTraceParams* params);

/**
 * Process image to extract contour with progress reporting
 * @param input_path Path to input image file
 * @param params Processing parameters (use print_trace_get_default_params if NULL)
 * @param contour Pointer to contour structure to fill (caller must free with print_trace_free_contour)
 * @param progress_callback Optional progress callback for UI updates
 * @param error_callback Optional error callback for detailed error reporting
 * @param user_data User context data passed to callbacks
 * @return PRINT_TRACE_SUCCESS if successful, error code otherwise
 */
PrintTraceResult print_trace_process_image_to_contour(
    const char* input_path,
    const PrintTraceParams* params,
    PrintTraceContour* contour,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
);

/**
 * Process image to a specific stage and output intermediate result
 * @param input_path Path to input image file
 * @param params Processing parameters (use print_trace_get_default_params if NULL)
 * @param target_stage Target stage to stop processing at
 * @param result_image Pointer to image data structure to fill with result (caller must free with print_trace_free_image_data)
 * @param contour Optional contour structure to fill if stage produces contour data (caller must free with print_trace_free_contour)
 * @param progress_callback Optional progress callback for UI updates
 * @param error_callback Optional error callback for detailed error reporting
 * @param user_data User context data passed to callbacks
 * @return PRINT_TRACE_SUCCESS if successful, error code otherwise
 */
PrintTraceResult print_trace_process_to_stage(
    const char* input_path,
    const PrintTraceParams* params,
    PrintTraceProcessingStage target_stage,
    PrintTraceImageData* result_image,
    PrintTraceContour* contour,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
);

/**
 * Save contour to DXF file
 * @param contour Pointer to contour data
 * @param output_path Path for output DXF file
 * @param error_callback Optional error callback
 * @param user_data User context data passed to error callback
 * @return PRINT_TRACE_SUCCESS if successful, error code otherwise
 */
PrintTraceResult print_trace_save_contour_to_dxf(
    const PrintTraceContour* contour,
    const char* output_path,
    PrintTraceErrorCallback error_callback,
    void* user_data
);

/**
 * Complete processing: image to DXF in one call
 * @param input_path Path to input image file
 * @param output_path Path for output DXF file
 * @param params Processing parameters (use print_trace_get_default_params if NULL)
 * @param progress_callback Optional progress callback for UI updates
 * @param error_callback Optional error callback for detailed error reporting
 * @param user_data User context data passed to callbacks
 * @return PRINT_TRACE_SUCCESS if successful, error code otherwise
 */
PrintTraceResult print_trace_process_image_to_dxf(
    const char* input_path,
    const char* output_path,
    const PrintTraceParams* params,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
);


// Memory management functions

/**
 * Free contour memory allocated by print_trace_process_image_to_contour
 * @param contour Pointer to contour to free
 */
void print_trace_free_contour(PrintTraceContour* contour);

/**
 * Free image data memory allocated by debug image functions
 * @param image_data Pointer to image data to free
 */
void print_trace_free_image_data(PrintTraceImageData* image_data);


// Utility functions

/**
 * Get human-readable error message for error code
 * @param error_code Error code from PrintTraceResult
 * @return Static string describing the error (do not free)
 */
const char* print_trace_get_error_message(PrintTraceResult error_code);

/**
 * Get library version string
 * @return Static version string in format "major.minor.patch" (do not free)
 */
const char* print_trace_get_version(void);

/**
 * Check if input file appears to be a valid image
 * @param file_path Path to image file
 * @return true if file appears to be a valid image, false otherwise
 */
bool print_trace_is_valid_image_file(const char* file_path);

/**
 * Get estimated processing time based on image size
 * @param image_path Path to image file
 * @return Estimated processing time in seconds, or -1.0 if image cannot be analyzed
 */
double print_trace_estimate_processing_time(const char* image_path);


/**
 * Get human-readable name for processing stage
 * @param stage Processing stage enum value
 * @return Static string describing the stage (do not free)
 */
const char* print_trace_get_processing_stage_name(PrintTraceProcessingStage stage);

/**
 * Get human-readable description for processing stage
 * @param stage Processing stage enum value
 * @return Static string describing what this stage shows (do not free)
 */
const char* print_trace_get_processing_stage_description(PrintTraceProcessingStage stage);

#ifdef __cplusplus
}
#endif

#endif // PRINT_TRACE_API_H
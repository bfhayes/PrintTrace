#ifndef OUTLINE_TOOL_API_H
#define OUTLINE_TOOL_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Version information
#define OUTLINE_TOOL_VERSION_MAJOR 1
#define OUTLINE_TOOL_VERSION_MINOR 0
#define OUTLINE_TOOL_VERSION_PATCH 0

// Error codes for Swift integration
typedef enum {
    OUTLINE_TOOL_SUCCESS = 0,
    OUTLINE_TOOL_ERROR_INVALID_INPUT = -1,
    OUTLINE_TOOL_ERROR_FILE_NOT_FOUND = -2,
    OUTLINE_TOOL_ERROR_IMAGE_LOAD_FAILED = -3,
    OUTLINE_TOOL_ERROR_IMAGE_TOO_SMALL = -4,
    OUTLINE_TOOL_ERROR_NO_CONTOURS = -5,
    OUTLINE_TOOL_ERROR_NO_BOUNDARY = -6,
    OUTLINE_TOOL_ERROR_NO_OBJECT = -7,
    OUTLINE_TOOL_ERROR_DXF_WRITE_FAILED = -8,
    OUTLINE_TOOL_ERROR_INVALID_PARAMETERS = -9,
    OUTLINE_TOOL_ERROR_PROCESSING_FAILED = -10
} OutlineToolResult;

// Processing parameters structure (CAD-optimized)
typedef struct {
    int32_t warp_size;              // Target image size after perspective correction (default: 3240)
    double real_world_size_mm;      // Real-world size in millimeters (default: 162.0)
    
    // Edge detection parameters (replacing threshold)
    double canny_lower;             // Canny lower threshold (default: 50.0)
    double canny_upper;             // Canny upper threshold (default: 150.0)
    int32_t canny_aperture;         // Canny aperture size (default: 3)
    
    // CLAHE parameters for lighting normalization  
    double clahe_clip_limit;        // CLAHE clip limit (default: 2.0)
    int32_t clahe_tile_size;        // CLAHE tile grid size (default: 8)
    
    // Contour filtering parameters
    double min_contour_area;        // Minimum contour area (default: 1000.0)
    double min_solidity;            // Minimum solidity ratio (default: 0.5)
    double max_aspect_ratio;        // Maximum aspect ratio (default: 10.0)
    
    // Polygon approximation (higher precision for CAD)
    double polygon_epsilon_factor;  // Polygon approximation factor (default: 0.005)
    
    // Sub-pixel refinement
    bool enable_subpixel_refinement; // Enable sub-pixel accuracy (default: true)
    int32_t corner_win_size;        // Corner refinement window size (default: 5)
    
    // Validation parameters
    bool validate_closed_contour;   // Validate contour closure (default: true)
    double min_perimeter;           // Minimum contour perimeter (default: 100.0)
    
    // Tolerance/dilation for 3D printing cases
    double dilation_amount_mm;      // Amount to dilate outline in millimeters (default: 0.0)
    
    // Smoothing for 3D printing
    bool enable_smoothing;          // Enable smoothing for easier 3D printing (default: false)
    double smoothing_amount_mm;     // Smoothing amount in millimeters (default: 0.2)
    
    // Debug visualization
    bool enable_debug_output;       // Enable debug image output (default: false)
} OutlineToolParams;

// Point structure for contour data
typedef struct {
    double x;
    double y;
} OutlineToolPoint;

// Contour data structure
typedef struct {
    OutlineToolPoint* points;
    int32_t point_count;
    double pixels_per_mm;
} OutlineToolContour;

// Progress callback function type for SwiftUI progress tracking
typedef void (*OutlineToolProgressCallback)(double progress, const char* stage);

// Error callback function type for detailed error reporting
typedef void (*OutlineToolErrorCallback)(OutlineToolResult error_code, const char* error_message);

// Core API Functions

/**
 * Get default processing parameters
 * @param params Pointer to parameters structure to fill
 */
void outline_tool_get_default_params(OutlineToolParams* params);

/**
 * Validate processing parameters
 * @param params Pointer to parameters to validate
 * @return OUTLINE_TOOL_SUCCESS if valid, error code otherwise
 */
OutlineToolResult outline_tool_validate_params(const OutlineToolParams* params);

/**
 * Process image to extract contour with progress reporting
 * @param input_path Path to input image file
 * @param params Processing parameters (use outline_tool_get_default_params if NULL)
 * @param contour Pointer to contour structure to fill (caller must free with outline_tool_free_contour)
 * @param progress_callback Optional progress callback for UI updates
 * @param error_callback Optional error callback for detailed error reporting
 * @return OUTLINE_TOOL_SUCCESS if successful, error code otherwise
 */
OutlineToolResult outline_tool_process_image_to_contour(
    const char* input_path,
    const OutlineToolParams* params,
    OutlineToolContour* contour,
    OutlineToolProgressCallback progress_callback,
    OutlineToolErrorCallback error_callback
);

/**
 * Save contour to DXF file
 * @param contour Pointer to contour data
 * @param output_path Path for output DXF file
 * @param error_callback Optional error callback
 * @return OUTLINE_TOOL_SUCCESS if successful, error code otherwise
 */
OutlineToolResult outline_tool_save_contour_to_dxf(
    const OutlineToolContour* contour,
    const char* output_path,
    OutlineToolErrorCallback error_callback
);

/**
 * Complete processing: image to DXF in one call
 * @param input_path Path to input image file
 * @param output_path Path for output DXF file
 * @param params Processing parameters (use outline_tool_get_default_params if NULL)
 * @param progress_callback Optional progress callback for UI updates
 * @param error_callback Optional error callback for detailed error reporting
 * @return OUTLINE_TOOL_SUCCESS if successful, error code otherwise
 */
OutlineToolResult outline_tool_process_image_to_dxf(
    const char* input_path,
    const char* output_path,
    const OutlineToolParams* params,
    OutlineToolProgressCallback progress_callback,
    OutlineToolErrorCallback error_callback
);

// Memory management functions

/**
 * Free contour memory allocated by outline_tool_process_image_to_contour
 * @param contour Pointer to contour to free
 */
void outline_tool_free_contour(OutlineToolContour* contour);

// Utility functions

/**
 * Get human-readable error message for error code
 * @param error_code Error code from OutlineToolResult
 * @return Static string describing the error (do not free)
 */
const char* outline_tool_get_error_message(OutlineToolResult error_code);

/**
 * Get library version string
 * @return Static version string in format "major.minor.patch" (do not free)
 */
const char* outline_tool_get_version(void);

/**
 * Check if input file appears to be a valid image
 * @param file_path Path to image file
 * @return true if file appears to be a valid image, false otherwise
 */
bool outline_tool_is_valid_image_file(const char* file_path);

/**
 * Get estimated processing time based on image size
 * @param image_path Path to image file
 * @return Estimated processing time in seconds, or -1.0 if image cannot be analyzed
 */
double outline_tool_estimate_processing_time(const char* image_path);

#ifdef __cplusplus
}
#endif

#endif // OUTLINE_TOOL_API_H
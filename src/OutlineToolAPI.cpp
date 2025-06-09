#include "OutlineToolAPI.h"
#include "ImageProcessor.hpp"
#include "DXFWriter.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

using namespace OutlineTool;

// Internal helper functions
namespace {
    
    // Convert C parameters to C++ parameters
    ImageProcessor::ProcessingParams convertParams(const OutlineToolParams* params) {
        ImageProcessor::ProcessingParams cpp_params;
        if (params) {
            cpp_params.warpSize = params->warp_size;
            cpp_params.realWorldSizeMM = params->real_world_size_mm;
            
            // New CAD-optimized parameters
            cpp_params.cannyLower = params->canny_lower;
            cpp_params.cannyUpper = params->canny_upper;
            cpp_params.cannyAperture = params->canny_aperture;
            
            cpp_params.claheClipLimit = params->clahe_clip_limit;
            cpp_params.claheTileSize = params->clahe_tile_size;
            
            cpp_params.minContourArea = params->min_contour_area;
            cpp_params.minSolidity = params->min_solidity;
            cpp_params.maxAspectRatio = params->max_aspect_ratio;
            
            cpp_params.polygonEpsilonFactor = params->polygon_epsilon_factor;
            
            cpp_params.enableSubPixelRefinement = params->enable_subpixel_refinement;
            cpp_params.cornerWinSize = params->corner_win_size;
            
            cpp_params.validateClosedContour = params->validate_closed_contour;
            cpp_params.minPerimeter = params->min_perimeter;
            
            cpp_params.dilationAmountMM = params->dilation_amount_mm;
            
            cpp_params.enableDebugOutput = params->enable_debug_output;
        }
        return cpp_params;
    }
    
    // Convert C++ contour to C contour
    void convertContour(const std::vector<cv::Point>& cpp_contour, double pixels_per_mm, OutlineToolContour* c_contour) {
        c_contour->point_count = static_cast<int32_t>(cpp_contour.size());
        c_contour->pixels_per_mm = pixels_per_mm;
        
        if (c_contour->point_count > 0) {
            c_contour->points = static_cast<OutlineToolPoint*>(malloc(sizeof(OutlineToolPoint) * c_contour->point_count));
            
            for (int i = 0; i < c_contour->point_count; i++) {
                c_contour->points[i].x = static_cast<double>(cpp_contour[i].x);
                c_contour->points[i].y = static_cast<double>(cpp_contour[i].y);
            }
        } else {
            c_contour->points = nullptr;
        }
    }
    
    // Convert C++ exception to error code
    OutlineToolResult handleException(const std::exception& e, OutlineToolErrorCallback error_callback) {
        if (error_callback) {
            error_callback(OUTLINE_TOOL_ERROR_PROCESSING_FAILED, e.what());
        }
        
        std::string what = e.what();
        if (what.find("Failed to load image") != std::string::npos) {
            return OUTLINE_TOOL_ERROR_IMAGE_LOAD_FAILED;
        } else if (what.find("too small") != std::string::npos) {
            return OUTLINE_TOOL_ERROR_IMAGE_TOO_SMALL;
        } else if (what.find("No contours found") != std::string::npos) {
            return OUTLINE_TOOL_ERROR_NO_CONTOURS;
        } else if (what.find("4 corners") != std::string::npos) {
            return OUTLINE_TOOL_ERROR_NO_BOUNDARY;
        } else if (what.find("No contours found for the object") != std::string::npos) {
            return OUTLINE_TOOL_ERROR_NO_OBJECT;
        }
        
        return OUTLINE_TOOL_ERROR_PROCESSING_FAILED;
    }
    
    // Progress reporting helper
    void reportProgress(OutlineToolProgressCallback callback, double progress, const char* stage) {
        if (callback) {
            callback(progress, stage);
        }
    }
}

// API Implementation

void outline_tool_get_default_params(OutlineToolParams* params) {
    if (!params) return;
    
    params->warp_size = 3240;
    params->real_world_size_mm = 162.0;
    
    // CAD-optimized edge detection parameters
    params->canny_lower = 50.0;
    params->canny_upper = 150.0;
    params->canny_aperture = 3;
    
    // CLAHE lighting normalization
    params->clahe_clip_limit = 2.0;
    params->clahe_tile_size = 8;
    
    // Enhanced contour filtering (more permissive for real images)
    params->min_contour_area = 500.0;
    params->min_solidity = 0.3;
    params->max_aspect_ratio = 20.0;
    
    // Higher precision polygon approximation for CAD
    params->polygon_epsilon_factor = 0.005;
    
    // Sub-pixel refinement for accuracy
    params->enable_subpixel_refinement = true;
    params->corner_win_size = 5;
    
    // Validation settings
    params->validate_closed_contour = true;
    params->min_perimeter = 100.0;
    
    // Tolerance/dilation settings
    params->dilation_amount_mm = 0.0;
    
    // Debug settings
    params->enable_debug_output = false;
}

OutlineToolResult outline_tool_validate_params(const OutlineToolParams* params) {
    if (!params) return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    
    // Basic parameters
    if (params->warp_size <= 0 || params->warp_size > 10000) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->real_world_size_mm <= 0.0 || params->real_world_size_mm > 1000.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Canny edge detection parameters
    if (params->canny_lower < 0.0 || params->canny_lower > 500.0 ||
        params->canny_upper < 0.0 || params->canny_upper > 500.0 ||
        params->canny_lower >= params->canny_upper) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->canny_aperture < 3 || params->canny_aperture > 7 || params->canny_aperture % 2 == 0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // CLAHE parameters
    if (params->clahe_clip_limit < 0.1 || params->clahe_clip_limit > 10.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->clahe_tile_size < 2 || params->clahe_tile_size > 32) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Contour filtering parameters
    if (params->min_contour_area < 10.0 || params->min_contour_area > 1000000.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->min_solidity < 0.1 || params->min_solidity > 1.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->max_aspect_ratio < 1.0 || params->max_aspect_ratio > 50.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Polygon approximation
    if (params->polygon_epsilon_factor < 0.0001 || params->polygon_epsilon_factor > 0.1) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Sub-pixel refinement
    if (params->corner_win_size < 3 || params->corner_win_size > 15) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Validation parameters
    if (params->min_perimeter < 10.0 || params->min_perimeter > 10000.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    // Dilation parameters
    if (params->dilation_amount_mm < 0.0 || params->dilation_amount_mm > 50.0) {
        return OUTLINE_TOOL_ERROR_INVALID_PARAMETERS;
    }
    
    return OUTLINE_TOOL_SUCCESS;
}

OutlineToolResult outline_tool_process_image_to_contour(
    const char* input_path,
    const OutlineToolParams* params,
    OutlineToolContour* contour,
    OutlineToolProgressCallback progress_callback,
    OutlineToolErrorCallback error_callback
) {
    if (!input_path || !contour) {
        if (error_callback) {
            error_callback(OUTLINE_TOOL_ERROR_INVALID_INPUT, "Invalid input parameters");
        }
        return OUTLINE_TOOL_ERROR_INVALID_INPUT;
    }
    
    // Initialize contour
    contour->points = nullptr;
    contour->point_count = 0;
    contour->pixels_per_mm = 0.0;
    
    // Check file exists
    std::ifstream file(input_path);
    if (!file.good()) {
        if (error_callback) {
            error_callback(OUTLINE_TOOL_ERROR_FILE_NOT_FOUND, "Input file not found or not readable");
        }
        return OUTLINE_TOOL_ERROR_FILE_NOT_FOUND;
    }
    
    // Validate parameters
    OutlineToolParams default_params;
    if (!params) {
        outline_tool_get_default_params(&default_params);
        params = &default_params;
    }
    
    OutlineToolResult validation_result = outline_tool_validate_params(params);
    if (validation_result != OUTLINE_TOOL_SUCCESS) {
        if (error_callback) {
            error_callback(validation_result, "Invalid processing parameters");
        }
        return validation_result;
    }
    
    try {
        reportProgress(progress_callback, 0.0, "Starting CAD-optimized image processing");
        
        // Convert parameters
        ImageProcessor::ProcessingParams cpp_params = convertParams(params);
        double pixels_per_mm = static_cast<double>(cpp_params.warpSize) / cpp_params.realWorldSizeMM;
        
        reportProgress(progress_callback, 0.1, "Loading and processing image");
        
        // Use the new improved pipeline
        std::vector<cv::Point> objectContour = ImageProcessor::processImageToContour(input_path, cpp_params);
        
        reportProgress(progress_callback, 0.9, "Converting contour data");
        
        // Convert contour to C format
        convertContour(objectContour, pixels_per_mm, contour);
        
        reportProgress(progress_callback, 1.0, "CAD-optimized processing complete");
        
        return OUTLINE_TOOL_SUCCESS;
        
    } catch (const std::invalid_argument& e) {
        return handleException(e, error_callback);
    } catch (const std::runtime_error& e) {
        return handleException(e, error_callback);
    } catch (const std::exception& e) {
        return handleException(e, error_callback);
    }
}

OutlineToolResult outline_tool_save_contour_to_dxf(
    const OutlineToolContour* contour,
    const char* output_path,
    OutlineToolErrorCallback error_callback
) {
    if (!contour || !output_path || !contour->points || contour->point_count <= 0) {
        if (error_callback) {
            error_callback(OUTLINE_TOOL_ERROR_INVALID_INPUT, "Invalid contour or output path");
        }
        return OUTLINE_TOOL_ERROR_INVALID_INPUT;
    }
    
    try {
        // Convert C contour back to C++ format
        std::vector<cv::Point> cpp_contour;
        cpp_contour.reserve(contour->point_count);
        
        for (int i = 0; i < contour->point_count; i++) {
            cpp_contour.emplace_back(
                static_cast<int>(contour->points[i].x),
                static_cast<int>(contour->points[i].y)
            );
        }
        
        // Save to DXF
        bool success = DXFWriter::saveContourAsDXF(cpp_contour, contour->pixels_per_mm, output_path);
        
        if (!success) {
            if (error_callback) {
                error_callback(OUTLINE_TOOL_ERROR_DXF_WRITE_FAILED, "Failed to write DXF file");
            }
            return OUTLINE_TOOL_ERROR_DXF_WRITE_FAILED;
        }
        
        return OUTLINE_TOOL_SUCCESS;
        
    } catch (const std::exception& e) {
        return handleException(e, error_callback);
    }
}

OutlineToolResult outline_tool_process_image_to_dxf(
    const char* input_path,
    const char* output_path,
    const OutlineToolParams* params,
    OutlineToolProgressCallback progress_callback,
    OutlineToolErrorCallback error_callback
) {
    if (!input_path || !output_path) {
        if (error_callback) {
            error_callback(OUTLINE_TOOL_ERROR_INVALID_INPUT, "Invalid input or output path");
        }
        return OUTLINE_TOOL_ERROR_INVALID_INPUT;
    }
    
    OutlineToolContour contour = {nullptr, 0, 0.0};
    
    // Process image to contour
    OutlineToolResult result = outline_tool_process_image_to_contour(
        input_path, params, &contour, progress_callback, error_callback
    );
    
    if (result != OUTLINE_TOOL_SUCCESS) {
        return result;
    }
    
    // Save contour to DXF
    result = outline_tool_save_contour_to_dxf(&contour, output_path, error_callback);
    
    // Clean up
    outline_tool_free_contour(&contour);
    
    return result;
}

void outline_tool_free_contour(OutlineToolContour* contour) {
    if (contour && contour->points) {
        free(contour->points);
        contour->points = nullptr;
        contour->point_count = 0;
        contour->pixels_per_mm = 0.0;
    }
}

const char* outline_tool_get_error_message(OutlineToolResult error_code) {
    switch (error_code) {
        case OUTLINE_TOOL_SUCCESS: return "Success";
        case OUTLINE_TOOL_ERROR_INVALID_INPUT: return "Invalid input parameters";
        case OUTLINE_TOOL_ERROR_FILE_NOT_FOUND: return "Input file not found or not readable";
        case OUTLINE_TOOL_ERROR_IMAGE_LOAD_FAILED: return "Failed to load image - check format and file integrity";
        case OUTLINE_TOOL_ERROR_IMAGE_TOO_SMALL: return "Image too small - minimum 100x100 pixels required";
        case OUTLINE_TOOL_ERROR_NO_CONTOURS: return "No contours found in image - ensure good contrast";
        case OUTLINE_TOOL_ERROR_NO_BOUNDARY: return "Could not detect rectangular boundary - ensure clear document edges";
        case OUTLINE_TOOL_ERROR_NO_OBJECT: return "No object found after processing - check image quality";
        case OUTLINE_TOOL_ERROR_DXF_WRITE_FAILED: return "Failed to write DXF file - check output path permissions";
        case OUTLINE_TOOL_ERROR_INVALID_PARAMETERS: return "Invalid processing parameters - check parameter ranges";
        case OUTLINE_TOOL_ERROR_PROCESSING_FAILED: return "Image processing failed - see error callback for details";
        default: return "Unknown error";
    }
}

const char* outline_tool_get_version(void) {
    return "1.0.0";
}

bool outline_tool_is_valid_image_file(const char* file_path) {
    if (!file_path) return false;
    
    try {
        cv::Mat img = cv::imread(file_path);
        return !img.empty();
    } catch (...) {
        return false;
    }
}

double outline_tool_estimate_processing_time(const char* image_path) {
    if (!image_path) return -1.0;
    
    try {
        cv::Mat img = cv::imread(image_path);
        if (img.empty()) return -1.0;
        
        // Rough estimation based on image size
        int64_t pixels = static_cast<int64_t>(img.rows) * img.cols;
        double base_time = 2.0; // Base time in seconds
        double pixel_factor = static_cast<double>(pixels) / (1920.0 * 1080.0); // Normalize to 1080p
        
        return base_time * (0.5 + 0.5 * pixel_factor);
        
    } catch (...) {
        return -1.0;
    }
}
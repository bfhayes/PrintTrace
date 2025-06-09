#include "PrintTraceAPI.h"
#include "ImageProcessor.hpp"
#include "DXFWriter.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>

using namespace PrintTrace;

// Internal helper functions
namespace {
    
    // Convert C parameters to C++ parameters
    ImageProcessor::ProcessingParams convertParams(const PrintTraceParams* params) {
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
            
            cpp_params.useAdaptiveThreshold = params->use_adaptive_threshold;
            cpp_params.manualThreshold = params->manual_threshold;
            cpp_params.thresholdOffset = params->threshold_offset;
            
            cpp_params.disableMorphology = params->disable_morphology;
            cpp_params.morphKernelSize = params->morph_kernel_size;
            
            cpp_params.mergeNearbyContours = params->merge_nearby_contours;
            cpp_params.contourMergeDistanceMM = params->contour_merge_distance_mm;
            
            cpp_params.minContourArea = params->min_contour_area;
            cpp_params.minSolidity = params->min_solidity;
            cpp_params.maxAspectRatio = params->max_aspect_ratio;
            
            cpp_params.polygonEpsilonFactor = params->polygon_epsilon_factor;
            
            cpp_params.enableSubPixelRefinement = params->enable_subpixel_refinement;
            cpp_params.cornerWinSize = params->corner_win_size;
            
            cpp_params.validateClosedContour = params->validate_closed_contour;
            cpp_params.minPerimeter = params->min_perimeter;
            
            cpp_params.dilationAmountMM = params->dilation_amount_mm;
            
            cpp_params.enableSmoothing = params->enable_smoothing;
            cpp_params.smoothingAmountMM = params->smoothing_amount_mm;
            
            cpp_params.enableDebugOutput = params->enable_debug_output;
        }
        return cpp_params;
    }
    
    // Convert C++ contour to C contour
    void convertContour(const std::vector<cv::Point>& cpp_contour, double pixels_per_mm, PrintTraceContour* c_contour) {
        c_contour->point_count = static_cast<int32_t>(cpp_contour.size());
        c_contour->pixels_per_mm = pixels_per_mm;
        
        if (c_contour->point_count > 0) {
            c_contour->points = static_cast<PrintTracePoint*>(malloc(sizeof(PrintTracePoint) * c_contour->point_count));
            
            for (int i = 0; i < c_contour->point_count; i++) {
                c_contour->points[i].x = static_cast<double>(cpp_contour[i].x);
                c_contour->points[i].y = static_cast<double>(cpp_contour[i].y);
            }
        } else {
            c_contour->points = nullptr;
        }
    }
    
    // Convert OpenCV Mat to PrintTraceImageData
    void convertMatToImageData(const cv::Mat& mat, PrintTraceImageData* image_data) {
        // Ensure we have a valid image
        if (mat.empty()) {
            image_data->data = nullptr;
            image_data->width = 0;
            image_data->height = 0;
            image_data->channels = 0;
            image_data->bytes_per_row = 0;
            return;
        }
        
        cv::Mat converted;
        
        // Convert to RGBA8888 format for Swift compatibility
        if (mat.channels() == 1) {
            // Grayscale to RGBA
            cv::cvtColor(mat, converted, cv::COLOR_GRAY2RGBA);
        } else if (mat.channels() == 3) {
            // BGR to RGBA
            cv::cvtColor(mat, converted, cv::COLOR_BGR2RGBA);
        } else if (mat.channels() == 4) {
            // Already RGBA, just copy
            converted = mat.clone();
        } else {
            throw std::runtime_error("Unsupported image format");
        }
        
        // Fill image data structure
        image_data->width = converted.cols;
        image_data->height = converted.rows;
        image_data->channels = 4; // Always RGBA
        image_data->bytes_per_row = converted.step;
        
        // Allocate and copy data
        size_t data_size = converted.total() * converted.elemSize();
        image_data->data = static_cast<uint8_t*>(malloc(data_size));
        std::memcpy(image_data->data, converted.data, data_size);
    }
    
    // Convert C++ exception to error code
    PrintTraceResult handleException(const std::exception& e, PrintTraceErrorCallback error_callback, void* user_data) {
        if (error_callback) {
            error_callback(PRINT_TRACE_ERROR_PROCESSING_FAILED, e.what(), user_data);
        }
        
        std::string what = e.what();
        if (what.find("Failed to load image") != std::string::npos) {
            return PRINT_TRACE_ERROR_IMAGE_LOAD_FAILED;
        } else if (what.find("too small") != std::string::npos) {
            return PRINT_TRACE_ERROR_IMAGE_TOO_SMALL;
        } else if (what.find("No contours found") != std::string::npos) {
            return PRINT_TRACE_ERROR_NO_CONTOURS;
        } else if (what.find("4 corners") != std::string::npos) {
            return PRINT_TRACE_ERROR_NO_BOUNDARY;
        } else if (what.find("No contours found for the object") != std::string::npos) {
            return PRINT_TRACE_ERROR_NO_OBJECT;
        }
        
        return PRINT_TRACE_ERROR_PROCESSING_FAILED;
    }
    
    // Progress reporting helper
    void reportProgress(PrintTraceProgressCallback callback, double progress, const char* stage, void* user_data) {
        if (callback) {
            callback(progress, stage, user_data);
        }
    }
}

// API Implementation

void print_trace_get_default_params(PrintTraceParams* params) {
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
    
    // Object detection parameters
    params->use_adaptive_threshold = false;
    params->manual_threshold = 0.0;        // 0 = automatic
    params->threshold_offset = 0.0;        // No offset from auto threshold
    
    // Morphological processing parameters
    params->disable_morphology = false;    // Enable morphological cleaning by default
    params->morph_kernel_size = 5;         // Moderate kernel size
    
    // Multi-contour detection parameters
    params->merge_nearby_contours = true;  // Enable contour merging by default
    params->contour_merge_distance_mm = 5.0; // 5mm merge distance
    
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
    
    // Smoothing settings
    params->enable_smoothing = false;
    params->smoothing_amount_mm = 0.2;
    
    // Debug settings
    params->enable_debug_output = false;
}

void print_trace_get_param_ranges(PrintTraceParamRanges* ranges) {
    if (!ranges) return;
    
    // Image processing parameter ranges
    ranges->warp_size_min = 500;
    ranges->warp_size_max = 8000;
    ranges->real_world_size_mm_min = 10.0;
    ranges->real_world_size_mm_max = 500.0;
    
    // Edge detection ranges
    ranges->canny_lower_min = 10.0;
    ranges->canny_lower_max = 200.0;
    ranges->canny_upper_min = 50.0;
    ranges->canny_upper_max = 400.0;
    ranges->canny_aperture_values[0] = 3;
    ranges->canny_aperture_values[1] = 5;
    ranges->canny_aperture_values[2] = 7;
    
    // CLAHE ranges
    ranges->clahe_clip_limit_min = 0.5;
    ranges->clahe_clip_limit_max = 8.0;
    ranges->clahe_tile_size_min = 4;
    ranges->clahe_tile_size_max = 16;
    
    // Object detection ranges
    ranges->manual_threshold_min = 0.0;   // 0 = auto
    ranges->manual_threshold_max = 255.0;
    ranges->threshold_offset_min = -50.0;
    ranges->threshold_offset_max = 50.0;
    ranges->morph_kernel_size_min = 3;
    ranges->morph_kernel_size_max = 15;
    ranges->contour_merge_distance_mm_min = 1.0;
    ranges->contour_merge_distance_mm_max = 20.0;
    
    // Contour filtering ranges
    ranges->min_contour_area_min = 100.0;
    ranges->min_contour_area_max = 10000.0;
    ranges->min_solidity_min = 0.1;
    ranges->min_solidity_max = 1.0;
    ranges->max_aspect_ratio_min = 2.0;
    ranges->max_aspect_ratio_max = 30.0;
    
    // Polygon approximation ranges
    ranges->polygon_epsilon_factor_min = 0.001;
    ranges->polygon_epsilon_factor_max = 0.02;
    
    // Sub-pixel refinement ranges
    ranges->corner_win_size_min = 3;
    ranges->corner_win_size_max = 15;
    
    // Validation ranges
    ranges->min_perimeter_min = 50.0;
    ranges->min_perimeter_max = 2000.0;
    
    // 3D printing ranges
    ranges->dilation_amount_mm_min = 0.0;
    ranges->dilation_amount_mm_max = 10.0;
    ranges->smoothing_amount_mm_min = 0.1;
    ranges->smoothing_amount_mm_max = 2.0;
}

PrintTraceResult print_trace_validate_params(const PrintTraceParams* params) {
    if (!params) return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    
    // Get ranges for validation
    PrintTraceParamRanges ranges;
    print_trace_get_param_ranges(&ranges);
    
    // Basic parameters
    if (params->warp_size < ranges.warp_size_min || params->warp_size > ranges.warp_size_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->real_world_size_mm < ranges.real_world_size_mm_min || 
        params->real_world_size_mm > ranges.real_world_size_mm_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Canny edge detection parameters
    if (params->canny_lower < ranges.canny_lower_min || params->canny_lower > ranges.canny_lower_max ||
        params->canny_upper < ranges.canny_upper_min || params->canny_upper > ranges.canny_upper_max ||
        params->canny_lower >= params->canny_upper) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Validate canny_aperture is one of the allowed values
    bool valid_aperture = false;
    for (int i = 0; i < 3; i++) {
        if (params->canny_aperture == ranges.canny_aperture_values[i]) {
            valid_aperture = true;
            break;
        }
    }
    if (!valid_aperture) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // CLAHE parameters
    if (params->clahe_clip_limit < ranges.clahe_clip_limit_min || 
        params->clahe_clip_limit > ranges.clahe_clip_limit_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->clahe_tile_size < ranges.clahe_tile_size_min || 
        params->clahe_tile_size > ranges.clahe_tile_size_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Object detection parameters
    if (params->manual_threshold < ranges.manual_threshold_min || 
        params->manual_threshold > ranges.manual_threshold_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->threshold_offset < ranges.threshold_offset_min || 
        params->threshold_offset > ranges.threshold_offset_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Morphological processing parameters
    if (params->morph_kernel_size < ranges.morph_kernel_size_min || 
        params->morph_kernel_size > ranges.morph_kernel_size_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Multi-contour detection parameters
    if (params->contour_merge_distance_mm < ranges.contour_merge_distance_mm_min || 
        params->contour_merge_distance_mm > ranges.contour_merge_distance_mm_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Contour filtering parameters
    if (params->min_contour_area < ranges.min_contour_area_min || 
        params->min_contour_area > ranges.min_contour_area_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->min_solidity < ranges.min_solidity_min || 
        params->min_solidity > ranges.min_solidity_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    if (params->max_aspect_ratio < ranges.max_aspect_ratio_min || 
        params->max_aspect_ratio > ranges.max_aspect_ratio_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Polygon approximation
    if (params->polygon_epsilon_factor < ranges.polygon_epsilon_factor_min || 
        params->polygon_epsilon_factor > ranges.polygon_epsilon_factor_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Sub-pixel refinement
    if (params->corner_win_size < ranges.corner_win_size_min || 
        params->corner_win_size > ranges.corner_win_size_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Validation parameters
    if (params->min_perimeter < ranges.min_perimeter_min || 
        params->min_perimeter > ranges.min_perimeter_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Dilation parameters
    if (params->dilation_amount_mm < ranges.dilation_amount_mm_min || 
        params->dilation_amount_mm > ranges.dilation_amount_mm_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    // Smoothing parameters
    if (params->smoothing_amount_mm < ranges.smoothing_amount_mm_min || 
        params->smoothing_amount_mm > ranges.smoothing_amount_mm_max) {
        return PRINT_TRACE_ERROR_INVALID_PARAMETERS;
    }
    
    return PRINT_TRACE_SUCCESS;
}

PrintTraceResult print_trace_process_image_to_contour(
    const char* input_path,
    const PrintTraceParams* params,
    PrintTraceContour* contour,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
) {
    // Simply delegate to print_trace_process_to_stage with FINAL stage
    // This eliminates code duplication and ensures consistency
    
    PrintTraceImageData dummy_image = {nullptr, 0, 0, 0, 0};
    
    PrintTraceResult result = print_trace_process_to_stage(
        input_path,
        params,
        PRINT_TRACE_STAGE_FINAL,
        &dummy_image,
        contour,
        progress_callback,
        error_callback,
        user_data
    );
    
    // Free the image data we don't need for this function
    if (dummy_image.data) {
        free(dummy_image.data);
    }
    
    return result;
}

PrintTraceResult print_trace_process_to_stage(
    const char* input_path,
    const PrintTraceParams* params,
    PrintTraceProcessingStage target_stage,
    PrintTraceImageData* result_image,
    PrintTraceContour* contour,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
) {
    if (!input_path || !result_image) {
        if (error_callback) {
            error_callback(PRINT_TRACE_ERROR_INVALID_INPUT, "Invalid input parameters", user_data);
        }
        return PRINT_TRACE_ERROR_INVALID_INPUT;
    }
    
    // Initialize outputs
    result_image->data = nullptr;
    result_image->width = 0;
    result_image->height = 0;
    result_image->channels = 0;
    result_image->bytes_per_row = 0;
    
    if (contour) {
        contour->points = nullptr;
        contour->point_count = 0;
        contour->pixels_per_mm = 0.0;
    }
    
    // Check file exists
    std::ifstream file(input_path);
    if (!file.good()) {
        if (error_callback) {
            error_callback(PRINT_TRACE_ERROR_FILE_NOT_FOUND, "Input file not found or not readable", user_data);
        }
        return PRINT_TRACE_ERROR_FILE_NOT_FOUND;
    }
    
    // Validate parameters
    PrintTraceParams default_params;
    if (!params) {
        print_trace_get_default_params(&default_params);
        params = &default_params;
    }
    
    PrintTraceResult validation_result = print_trace_validate_params(params);
    if (validation_result != PRINT_TRACE_SUCCESS) {
        if (error_callback) {
            error_callback(validation_result, "Invalid processing parameters", user_data);
        }
        return validation_result;
    }
    
    try {
        std::string stage_name = print_trace_get_processing_stage_name(target_stage);
        reportProgress(progress_callback, 0.0, ("Processing to stage: " + stage_name).c_str(), user_data);
        
        // Convert parameters
        ImageProcessor::ProcessingParams cpp_params = convertParams(params);
        
        // Process to target stage
        auto [result_mat, result_contour] = ImageProcessor::processImageToStage(
            input_path, cpp_params, static_cast<int>(target_stage)
        );
        
        reportProgress(progress_callback, 0.8, "Converting result data", user_data);
        
        // Convert result image
        convertMatToImageData(result_mat, result_image);
        
        // Convert contour if available and requested
        if (contour && !result_contour.empty()) {
            double pixels_per_mm = static_cast<double>(cpp_params.warpSize) / cpp_params.realWorldSizeMM;
            convertContour(result_contour, pixels_per_mm, contour);
        }
        
        reportProgress(progress_callback, 1.0, ("Processing to " + stage_name + " complete").c_str(), user_data);
        
        return PRINT_TRACE_SUCCESS;
        
    } catch (const std::invalid_argument& e) {
        return handleException(e, error_callback, user_data);
    } catch (const std::runtime_error& e) {
        return handleException(e, error_callback, user_data);
    } catch (const std::exception& e) {
        return handleException(e, error_callback, user_data);
    }
}

PrintTraceResult print_trace_save_contour_to_dxf(
    const PrintTraceContour* contour,
    const char* output_path,
    PrintTraceErrorCallback error_callback,
    void* user_data
) {
    if (!contour || !output_path || !contour->points || contour->point_count <= 0) {
        if (error_callback) {
            error_callback(PRINT_TRACE_ERROR_INVALID_INPUT, "Invalid contour or output path", user_data);
        }
        return PRINT_TRACE_ERROR_INVALID_INPUT;
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
                error_callback(PRINT_TRACE_ERROR_DXF_WRITE_FAILED, "Failed to write DXF file", user_data);
            }
            return PRINT_TRACE_ERROR_DXF_WRITE_FAILED;
        }
        
        return PRINT_TRACE_SUCCESS;
        
    } catch (const std::exception& e) {
        return handleException(e, error_callback, user_data);
    }
}

PrintTraceResult print_trace_process_image_to_dxf(
    const char* input_path,
    const char* output_path,
    const PrintTraceParams* params,
    PrintTraceProgressCallback progress_callback,
    PrintTraceErrorCallback error_callback,
    void* user_data
) {
    if (!input_path || !output_path) {
        if (error_callback) {
            error_callback(PRINT_TRACE_ERROR_INVALID_INPUT, "Invalid input or output path", user_data);
        }
        return PRINT_TRACE_ERROR_INVALID_INPUT;
    }
    
    PrintTraceContour contour = {nullptr, 0, 0.0};
    
    // Process image to contour
    PrintTraceResult result = print_trace_process_image_to_contour(
        input_path, params, &contour, progress_callback, error_callback, user_data
    );
    
    if (result != PRINT_TRACE_SUCCESS) {
        return result;
    }
    
    // Save contour to DXF
    result = print_trace_save_contour_to_dxf(&contour, output_path, error_callback, user_data);
    
    // Clean up
    print_trace_free_contour(&contour);
    
    return result;
}

void print_trace_free_contour(PrintTraceContour* contour) {
    if (contour && contour->points) {
        free(contour->points);
        contour->points = nullptr;
        contour->point_count = 0;
        contour->pixels_per_mm = 0.0;
    }
}

void print_trace_free_image_data(PrintTraceImageData* image_data) {
    if (image_data && image_data->data) {
        free(image_data->data);
        image_data->data = nullptr;
        image_data->width = 0;
        image_data->height = 0;
        image_data->channels = 0;
        image_data->bytes_per_row = 0;
    }
}

const char* print_trace_get_error_message(PrintTraceResult error_code) {
    switch (error_code) {
        case PRINT_TRACE_SUCCESS: return "Success";
        case PRINT_TRACE_ERROR_INVALID_INPUT: return "Invalid input parameters";
        case PRINT_TRACE_ERROR_FILE_NOT_FOUND: return "Input file not found or not readable";
        case PRINT_TRACE_ERROR_IMAGE_LOAD_FAILED: return "Failed to load image - check format and file integrity";
        case PRINT_TRACE_ERROR_IMAGE_TOO_SMALL: return "Image too small - minimum 100x100 pixels required";
        case PRINT_TRACE_ERROR_NO_CONTOURS: return "No contours found in image - ensure good contrast";
        case PRINT_TRACE_ERROR_NO_BOUNDARY: return "Could not detect rectangular boundary - ensure clear document edges";
        case PRINT_TRACE_ERROR_NO_OBJECT: return "No object found after processing - check image quality";
        case PRINT_TRACE_ERROR_DXF_WRITE_FAILED: return "Failed to write DXF file - check output path permissions";
        case PRINT_TRACE_ERROR_INVALID_PARAMETERS: return "Invalid processing parameters - check parameter ranges";
        case PRINT_TRACE_ERROR_PROCESSING_FAILED: return "Image processing failed - see error callback for details";
        default: return "Unknown error";
    }
}

const char* print_trace_get_version(void) {
    return "1.0.0";
}

bool print_trace_is_valid_image_file(const char* file_path) {
    if (!file_path) return false;
    
    try {
        cv::Mat img = cv::imread(file_path);
        return !img.empty();
    } catch (...) {
        return false;
    }
}

double print_trace_estimate_processing_time(const char* image_path) {
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

const char* print_trace_get_processing_stage_name(PrintTraceProcessingStage stage) {
    switch (stage) {
        case PRINT_TRACE_STAGE_LOADED: return "Loaded";
        case PRINT_TRACE_STAGE_LIGHTBOX_CROPPED: return "Lightbox Cropped";
        case PRINT_TRACE_STAGE_NORMALIZED: return "Normalized";
        case PRINT_TRACE_STAGE_BOUNDARY_DETECTED: return "Boundary Detected";
        case PRINT_TRACE_STAGE_OBJECT_DETECTED: return "Object Detected";
        case PRINT_TRACE_STAGE_SMOOTHED: return "Smoothed";
        case PRINT_TRACE_STAGE_DILATED: return "Dilated";
        case PRINT_TRACE_STAGE_FINAL: return "Final";
        default: return "Unknown Stage";
    }
}

const char* print_trace_get_processing_stage_description(PrintTraceProcessingStage stage) {
    switch (stage) {
        case PRINT_TRACE_STAGE_LOADED: 
            return "Image loaded and converted to grayscale";
        case PRINT_TRACE_STAGE_LIGHTBOX_CROPPED: 
            return "Perspective corrected to lightbox area - all subsequent images have uniform dimensions";
        case PRINT_TRACE_STAGE_NORMALIZED: 
            return "Lighting normalized using CLAHE for better contrast";
        case PRINT_TRACE_STAGE_BOUNDARY_DETECTED: 
            return "Lightbox boundary contour found and refined";
        case PRINT_TRACE_STAGE_OBJECT_DETECTED: 
            return "Main object contour extracted from warped image";
        case PRINT_TRACE_STAGE_SMOOTHED: 
            return "Contour smoothed for easier 3D printing (if enabled)";
        case PRINT_TRACE_STAGE_DILATED: 
            return "Contour dilated for manufacturing tolerance (if enabled)";
        case PRINT_TRACE_STAGE_FINAL: 
            return "Final validated contour ready for DXF export";
        default: 
            return "Unknown processing stage";
    }
}
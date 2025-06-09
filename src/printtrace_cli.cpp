#include <PrintTraceAPI.h>
#include <iostream>
#include <string>
#include <fstream>
#include <cstring>

using namespace std;

struct Arguments {
    string inputPath;
    string outputPath;
    bool valid = false;
    bool verbose = false;
    bool debug = false;
    double dilationMM = 0.0;
    bool enableSmoothing = false;
    double smoothingMM = 0.2;
    int smoothingMode = 1;             // Default to curvature-based
    
    // Object detection parameters
    bool useAdaptiveThreshold = false;
    double manualThreshold = 0.0;      // 0 = auto
    double thresholdOffset = 0.0;      // Offset from auto threshold
    
    // Morphological processing parameters
    bool disableMorphology = false;    // Disable morphological cleaning
    int morphKernelSize = 5;          // Morphological kernel size
    
    // Multi-contour detection parameters  
    bool disableContourMerging = false; // Disable contour merging
    double contourMergeDistance = 5.0;  // Contour merge distance in mm
};

Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;
    
    if (argc < 2) {
        return args;
    }

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && (i + 1 < argc)) {
            args.inputPath = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && (i + 1 < argc)) {
            args.outputPath = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "-d" || arg == "--debug") {
            args.debug = true;
        } else if ((arg == "-t" || arg == "--tolerance") && (i + 1 < argc)) {
            args.dilationMM = stod(argv[++i]);
        } else if (arg == "-s" || arg == "--smooth") {
            args.enableSmoothing = true;
        } else if ((arg == "--smooth-amount") && (i + 1 < argc)) {
            args.smoothingMM = stod(argv[++i]);
            args.enableSmoothing = true; // Auto-enable when amount is specified
        } else if ((arg == "--smooth-mode") && (i + 1 < argc)) {
            args.smoothingMode = stoi(argv[++i]);
        } else if (arg == "--adaptive-threshold") {
            args.useAdaptiveThreshold = true;
        } else if ((arg == "--manual-threshold") && (i + 1 < argc)) {
            args.manualThreshold = stod(argv[++i]);
        } else if ((arg == "--threshold-offset") && (i + 1 < argc)) {
            args.thresholdOffset = stod(argv[++i]);
        } else if (arg == "--disable-morphology") {
            args.disableMorphology = true;
        } else if ((arg == "--morph-kernel-size") && (i + 1 < argc)) {
            args.morphKernelSize = stoi(argv[++i]);
        } else if (arg == "--disable-contour-merging") {
            args.disableContourMerging = true;
        } else if ((arg == "--contour-merge-distance") && (i + 1 < argc)) {
            args.contourMergeDistance = stod(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            return args; // Will trigger usage display
        }
    }

    if (args.inputPath.empty()) {
        return args;
    }

    // Auto-generate output path if not provided
    if (args.outputPath.empty()) {
        size_t dotPos = args.inputPath.find_last_of('.');
        if (dotPos == string::npos) {
            args.outputPath = args.inputPath + ".dxf";
        } else {
            args.outputPath = args.inputPath.substr(0, dotPos) + ".dxf";
        }
    }

    args.valid = true;
    return args;
}

void printUsage(const char* progName) {
    cout << "PrintTrace CLI - Convert photos to DXF outlines for 3D printing\n"
         << "Using libprinttrace v" << print_trace_get_version() << "\n"
         << "\n"
         << "Usage: " << progName << " -i <input_image> [-o <output_dxf>] [options]\n"
         << "\n"
         << "Required:\n"
         << "  -i, --input   Input image file path\n"
         << "\n"
         << "Optional:\n"
         << "  -o, --output  Output DXF file path (auto-generated if not specified)\n"
         << "  -t, --tolerance <mm>  Add tolerance/clearance in millimeters for 3D printing (default: 0.0)\n"
         << "  -s, --smooth  Enable smoothing to remove small details for easier 3D printing\n"
         << "  --smooth-amount <mm>  Smoothing amount in millimeters (default: 0.2, enables smoothing)\n"
         << "  --smooth-mode <0|1>  Smoothing algorithm: 0=morphological (legacy), 1=curvature-based (default)\n"
         << "\n"
         << "Object Detection:\n"
         << "  --adaptive-threshold  Use adaptive thresholding instead of Otsu (better for uneven lighting)\n"
         << "  --manual-threshold <0-255>  Manual threshold value (0 = auto, overrides Otsu)\n"
         << "  --threshold-offset <-50 to +50>  Adjust Otsu threshold by this amount (negative = more inclusive)\n"
         << "  --disable-morphology  Disable morphological cleaning (preserves more peripheral detail)\n"
         << "  --morph-kernel-size <3-15>  Size of morphological kernel (smaller = less aggressive cleaning)\n"
         << "  --disable-contour-merging  Disable multi-contour merging (use single largest contour only)\n"
         << "  --contour-merge-distance <1-20>  Max distance in mm to merge object parts (default: 5.0)\n"
         << "\n"
         << "General:\n"
         << "  -v, --verbose Enable verbose output\n"
         << "  -d, --debug   Enable debug visualization (saves step-by-step images)\n"
         << "  -h, --help    Show this help message\n"
         << "\n"
         << "Examples:\n"
         << "  " << progName << " -i photo.jpg\n"
         << "  " << progName << " -i photo.jpg -o drawing.dxf\n"
         << "  " << progName << " -i photo.jpg -t 0.5  # Add 0.5mm tolerance for 3D printing\n"
         << "  " << progName << " -i photo.jpg -s      # Enable smoothing for easier printing\n"
         << "  " << progName << " -i photo.jpg -s -t 1.0  # Smooth + 1mm tolerance\n"
         << "  " << progName << " -i photo.jpg -s --smooth-mode 0  # Use legacy morphological smoothing\n"
         << "  " << progName << " -i photo.jpg -s --smooth-mode 1  # Use curvature-based smoothing (default)\n"
         << "  " << progName << " -i photo.jpg --threshold-offset -15  # More inclusive thresholding\n"
         << "  " << progName << " -i photo.jpg --manual-threshold 120  # Use specific threshold value\n"
         << "  " << progName << " -i photo.jpg --adaptive-threshold    # Better for uneven lighting\n"
         << "  " << progName << " -i photo.jpg --disable-morphology    # Preserve peripheral detail\n"
         << "  " << progName << " -i photo.jpg --morph-kernel-size 3   # Gentle morphological cleaning\n"
         << "  " << progName << " -i photo.jpg --disable-contour-merging # Use single largest contour only\n"
         << "  " << progName << " -i photo.jpg --contour-merge-distance 2.0 # Merge parts within 2mm\n"
         << "  " << progName << " -i photo.jpg -v\n"
         << "  " << progName << " -i photo.jpg -d  # Saves debug images to ./debug/\n"
         << endl;
}

// Progress callback for verbose mode
void progressCallback(double progress, const char* stage, void* user_data) {
    cout << "[PROGRESS] " << stage << ": " << (int)(progress * 100) << "%" << endl;
}

// Error callback for detailed error reporting
void errorCallback(PrintTraceResult error_code, const char* error_message, void* user_data) {
    cerr << "[ERROR] Code " << error_code << ": " << error_message << endl;
}

int main(int argc, char* argv[]) {
    Arguments args = parseArguments(argc, argv);
    
    if (!args.valid) {
        printUsage(argv[0]);
        return 1;
    }

    if (args.verbose) {
        cout << "[INFO] PrintTrace CLI v" << print_trace_get_version() << endl;
        cout << "[INFO] Processing: " << args.inputPath << " -> " << args.outputPath << endl;
    }

    // Validate input file
    if (!print_trace_is_valid_image_file(args.inputPath.c_str())) {
        cerr << "[ERROR] Input file is not a valid image or does not exist: " << args.inputPath << endl;
        return 1;
    }

    // Check if input file is readable
    if (!std::ifstream(args.inputPath).good()) {
        cerr << "[ERROR] Input file is not readable: " << args.inputPath << endl;
        return 1;
    }

    // Get default parameters
    PrintTraceParams params;
    print_trace_get_default_params(&params);
    
    // Enable debug output if requested
    if (args.debug) {
        params.enable_debug_output = true;
        cout << "[INFO] Debug mode enabled - images will be saved to ./debug/" << endl;
    }
    
    // Set tolerance for 3D printing if requested
    if (args.dilationMM > 0.0) {
        params.dilation_amount_mm = args.dilationMM;
        cout << "[INFO] 3D printing tolerance enabled: " << args.dilationMM << "mm" << endl;
    }
    
    // Set smoothing for 3D printing if requested
    if (args.enableSmoothing) {
        params.enable_smoothing = true;
        params.smoothing_amount_mm = args.smoothingMM;
        params.smoothing_mode = args.smoothingMode;
        cout << "[INFO] 3D printing smoothing enabled: " << args.smoothingMM << "mm using " 
             << (args.smoothingMode == 0 ? "morphological" : "curvature-based") << " method" << endl;
    }
    
    // Set object detection parameters if requested
    if (args.useAdaptiveThreshold) {
        params.use_adaptive_threshold = true;
        cout << "[INFO] Using adaptive thresholding for object detection" << endl;
    }
    
    if (args.manualThreshold > 0.0) {
        params.manual_threshold = args.manualThreshold;
        cout << "[INFO] Using manual threshold: " << args.manualThreshold << endl;
    }
    
    if (args.thresholdOffset != 0.0) {
        params.threshold_offset = args.thresholdOffset;
        cout << "[INFO] Threshold offset: " << (args.thresholdOffset > 0 ? "+" : "") << args.thresholdOffset 
             << " (more " << (args.thresholdOffset < 0 ? "inclusive" : "exclusive") << ")" << endl;
    }
    
    if (args.disableMorphology) {
        params.disable_morphology = true;
        cout << "[INFO] Morphological cleaning disabled - preserving peripheral detail" << endl;
    }
    
    if (args.morphKernelSize != 5) {
        params.morph_kernel_size = args.morphKernelSize;
        cout << "[INFO] Morphological kernel size: " << args.morphKernelSize 
             << " (" << (args.morphKernelSize < 5 ? "gentler" : "more aggressive") << " cleaning)" << endl;
    }
    
    if (args.disableContourMerging) {
        params.merge_nearby_contours = false;
        cout << "[INFO] Contour merging disabled - using single largest contour only" << endl;
    }
    
    if (args.contourMergeDistance != 5.0) {
        params.contour_merge_distance_mm = args.contourMergeDistance;
        cout << "[INFO] Contour merge distance: " << args.contourMergeDistance << "mm" << endl;
    }

    // Validate parameters
    PrintTraceResult validation_result = print_trace_validate_params(&params);
    if (validation_result != PRINT_TRACE_SUCCESS) {
        cerr << "[ERROR] Invalid default parameters: " << print_trace_get_error_message(validation_result) << endl;
        return 1;
    }

    if (args.verbose) {
        cout << "[INFO] Using CAD-optimized parameters:" << endl;
        cout << "  Lightbox size: " << params.lightbox_width_px << "x" << params.lightbox_height_px << "px" << endl;
        cout << "  Real world size: " << params.lightbox_width_mm << "x" << params.lightbox_height_mm << "mm" << endl;
        cout << "  Canny edges: " << params.canny_lower << "-" << params.canny_upper << endl;
        cout << "  CLAHE clip limit: " << params.clahe_clip_limit << endl;
        cout << "  Object detection: ";
        if (params.use_adaptive_threshold) {
            cout << "adaptive threshold";
        } else if (params.manual_threshold > 0.0) {
            cout << "manual threshold (" << params.manual_threshold << ")";
        } else {
            cout << "Otsu auto-threshold";
            if (params.threshold_offset != 0.0) {
                cout << " with offset (" << (params.threshold_offset > 0 ? "+" : "") << params.threshold_offset << ")";
            }
        }
        cout << endl;
        cout << "  Min contour area: " << params.min_contour_area << endl;
        cout << "  Min solidity: " << params.min_solidity << endl;
        cout << "  Polygon epsilon: " << params.polygon_epsilon_factor << endl;
        cout << "  Sub-pixel refinement: " << (params.enable_subpixel_refinement ? "enabled" : "disabled") << endl;
        cout << "  3D printing tolerance: " << params.dilation_amount_mm << "mm" << endl;
        cout << "  3D printing smoothing: " << (params.enable_smoothing ? "enabled" : "disabled");
        if (params.enable_smoothing) {
            cout << " (" << params.smoothing_amount_mm << "mm)";
        }
        cout << endl;
        
        double estimated_time = print_trace_estimate_processing_time(args.inputPath.c_str());
        if (estimated_time > 0) {
            cout << "  Estimated time: " << (int)estimated_time << "s" << endl;
        }
    }

    // Process image to DXF
    PrintTraceResult result = print_trace_process_image_to_dxf(
        args.inputPath.c_str(),
        args.outputPath.c_str(),
        &params,
        args.verbose ? progressCallback : nullptr,
        args.verbose ? errorCallback : nullptr,
        nullptr  // No user data needed for CLI
    );

    if (result == PRINT_TRACE_SUCCESS) {
        cout << "[SUCCESS] Conversion completed successfully!" << endl;
        cout << "[INFO] Output saved to: " << args.outputPath << endl;
        return 0;
    } else {
        const char* error_msg = print_trace_get_error_message(result);
        cerr << "[ERROR] Processing failed: " << error_msg << endl;
        return 1;
    }
}
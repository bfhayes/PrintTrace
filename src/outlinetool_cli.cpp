#include <OutlineToolAPI.h>
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
    cout << "OutlineTool CLI - Convert image outlines to DXF format\n"
         << "Using liboutlinetool v" << outline_tool_get_version() << "\n"
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
         << "  " << progName << " -i photo.jpg -v\n"
         << "  " << progName << " -i photo.jpg -d  # Saves debug images to ./debug/\n"
         << endl;
}

// Progress callback for verbose mode
void progressCallback(double progress, const char* stage) {
    cout << "[PROGRESS] " << stage << ": " << (int)(progress * 100) << "%" << endl;
}

// Error callback for detailed error reporting
void errorCallback(OutlineToolResult error_code, const char* error_message) {
    cerr << "[ERROR] Code " << error_code << ": " << error_message << endl;
}

int main(int argc, char* argv[]) {
    Arguments args = parseArguments(argc, argv);
    
    if (!args.valid) {
        printUsage(argv[0]);
        return 1;
    }

    if (args.verbose) {
        cout << "[INFO] OutlineTool CLI v" << outline_tool_get_version() << endl;
        cout << "[INFO] Processing: " << args.inputPath << " -> " << args.outputPath << endl;
    }

    // Validate input file
    if (!outline_tool_is_valid_image_file(args.inputPath.c_str())) {
        cerr << "[ERROR] Input file is not a valid image or does not exist: " << args.inputPath << endl;
        return 1;
    }

    // Check if input file is readable
    if (!std::ifstream(args.inputPath).good()) {
        cerr << "[ERROR] Input file is not readable: " << args.inputPath << endl;
        return 1;
    }

    // Get default parameters
    OutlineToolParams params;
    outline_tool_get_default_params(&params);
    
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
        cout << "[INFO] 3D printing smoothing enabled: " << args.smoothingMM << "mm" << endl;
    }

    // Validate parameters
    OutlineToolResult validation_result = outline_tool_validate_params(&params);
    if (validation_result != OUTLINE_TOOL_SUCCESS) {
        cerr << "[ERROR] Invalid default parameters: " << outline_tool_get_error_message(validation_result) << endl;
        return 1;
    }

    if (args.verbose) {
        cout << "[INFO] Using CAD-optimized parameters:" << endl;
        cout << "  Warp size: " << params.warp_size << "px" << endl;
        cout << "  Real world size: " << params.real_world_size_mm << "mm" << endl;
        cout << "  Canny edges: " << params.canny_lower << "-" << params.canny_upper << endl;
        cout << "  CLAHE clip limit: " << params.clahe_clip_limit << endl;
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
        
        double estimated_time = outline_tool_estimate_processing_time(args.inputPath.c_str());
        if (estimated_time > 0) {
            cout << "  Estimated time: " << (int)estimated_time << "s" << endl;
        }
    }

    // Process image to DXF
    OutlineToolResult result = outline_tool_process_image_to_dxf(
        args.inputPath.c_str(),
        args.outputPath.c_str(),
        &params,
        args.verbose ? progressCallback : nullptr,
        args.verbose ? errorCallback : nullptr
    );

    if (result == OUTLINE_TOOL_SUCCESS) {
        cout << "[SUCCESS] Conversion completed successfully!" << endl;
        cout << "[INFO] Output saved to: " << args.outputPath << endl;
        return 0;
    } else {
        const char* error_msg = outline_tool_get_error_message(result);
        cerr << "[ERROR] Processing failed: " << error_msg << endl;
        return 1;
    }
}
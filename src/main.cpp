#include "ImageProcessor.hpp"
#include "DXFWriter.hpp"
#include <iostream>
#include <string>
#include <exception>
#include <fstream>

using namespace std;
using namespace OutlineTool;

struct Arguments {
    string inputPath;
    string outputPath;
    bool valid = false;
};

Arguments parseArguments(int argc, char* argv[]) {
    Arguments args;
    
    if (argc < 3) {
        return args;
    }

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && (i + 1 < argc)) {
            args.inputPath = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && (i + 1 < argc)) {
            args.outputPath = argv[++i];
        }
    }

    if (args.inputPath.empty()) {
        return args;
    }

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
    cout << "OutlineTool - Convert image outlines to DXF format\n"
         << "\n"
         << "Usage: " << progName << " -i <input_image> [-o <output_dxf>]\n"
         << "\n"
         << "Options:\n"
         << "  -i, --input   Input image file path\n"
         << "  -o, --output  Output DXF file path (optional)\n"
         << "\n"
         << "If output path is not specified, it will be generated from the input filename.\n"
         << endl;
}

int main(int argc, char* argv[]) {
    Arguments args = parseArguments(argc, argv);
    
    if (!args.valid) {
        printUsage(argv[0]);
        return 1;
    }

    cout << "[INFO] Processing: " << args.inputPath << " -> " << args.outputPath << endl;

    try {
        // Validate input file exists and is readable
        if (!std::ifstream(args.inputPath).good()) {
            cerr << "[ERROR] Input file does not exist or is not readable: " << args.inputPath << endl;
            return 1;
        }

        // Check output directory is writable
        string outputDir = args.outputPath.substr(0, args.outputPath.find_last_of("/\\"));
        if (outputDir.empty()) outputDir = ".";
        
        ImageProcessor::ProcessingParams params;
        vector<cv::Point> contour = ImageProcessor::processImageToContour(args.inputPath, params);
        
        if (contour.empty()) {
            cerr << "[ERROR] No contour found in the processed image" << endl;
            return 1;
        }
        
        double pixelsPerMM = static_cast<double>(params.warpSize) / params.realWorldSizeMM;
        
        if (!DXFWriter::saveContourAsDXF(contour, pixelsPerMM, args.outputPath)) {
            cerr << "[ERROR] Failed to save DXF file." << endl;
            return 1;
        }

        cout << "[SUCCESS] Conversion completed successfully!" << endl;
        cout << "[INFO] Output saved to: " << args.outputPath << endl;
        
    } catch (const invalid_argument& e) {
        cerr << "[ERROR] Invalid argument: " << e.what() << endl;
        return 1;
    } catch (const runtime_error& e) {
        cerr << "[ERROR] Processing failed: " << e.what() << endl;
        return 1;
    } catch (const exception& e) {
        cerr << "[ERROR] Unexpected error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
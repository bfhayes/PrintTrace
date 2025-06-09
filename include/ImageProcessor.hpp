#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

namespace OutlineTool {

class ImageProcessor {
public:
    struct ProcessingParams {
        int warpSize = 3240;
        double realWorldSizeMM = 162.0;
        
        // Edge detection parameters (replacing threshold)
        double cannyLower = 50.0;
        double cannyUpper = 150.0;
        int cannyAperture = 3;
        
        // CLAHE parameters for lighting normalization
        double claheClipLimit = 2.0;
        int claheTileSize = 8;
        
        // Contour filtering parameters (more permissive for real images)
        double minContourArea = 500.0;
        double minSolidity = 0.3;
        double maxAspectRatio = 20.0;
        
        // Polygon approximation
        double polygonEpsilonFactor = 0.005;  // Reduced for higher precision
        
        // Sub-pixel refinement
        bool enableSubPixelRefinement = true;
        int cornerWinSize = 5;
        int cornerZeroZone = -1;
        
        // Validation parameters
        bool validateClosedContour = true;
        double minPerimeter = 100.0;
        
        // Tolerance/dilation for 3D printing cases
        double dilationAmountMM = 0.0;  // Amount to dilate outline in millimeters (0 = no dilation)
        
        // Smoothing for 3D printing
        bool enableSmoothing = false;    // Enable smoothing for easier 3D printing
        double smoothingAmountMM = 0.2;  // Smoothing amount in millimeters (removes details smaller than this)
        
        // Debug visualization
        bool enableDebugOutput = false;
        std::string debugOutputPath = "./debug/";
    };

    static cv::Mat loadImage(const std::string& path);
    static cv::Mat convertToGrayscale(const cv::Mat& img);
    
    // New improved pipeline methods
    static cv::Mat normalizeLighting(const cv::Mat& grayImg, const ProcessingParams& params);
    static cv::Mat detectEdges(const cv::Mat& grayImg, const ProcessingParams& params);
    static cv::Mat detectLightboxBoundary(const cv::Mat& grayImg, const ProcessingParams& params);
    static std::vector<cv::Point> findBoundaryContour(const cv::Mat& edgeImg, const ProcessingParams& params);
    static std::vector<cv::Point2f> refineCorners(const std::vector<cv::Point>& corners, 
                                                  const cv::Mat& grayImg, 
                                                  const ProcessingParams& params);
    
    static std::pair<cv::Mat, double> warpImage(const cv::Mat& originalImg, 
                                               const std::vector<cv::Point2f>& corners,
                                               int side, double realWorldSizeMM);
    
    // Legacy warpImage for backward compatibility
    static std::pair<cv::Mat, double> warpImage(const cv::Mat& binaryImg, 
                                               const std::vector<cv::Point>& approx,
                                               int side, double realWorldSizeMM);
    
    static std::vector<cv::Point> findObjectContour(const cv::Mat& warpedImg, const ProcessingParams& params);
    static std::vector<cv::Point2f> refineContour(const std::vector<cv::Point>& contour,
                                                  const cv::Mat& grayImg,
                                                  const ProcessingParams& params);
    static std::vector<cv::Point> dilateContour(const std::vector<cv::Point>& contour, 
                                                double dilationMM, double pixelsPerMM,
                                                const ProcessingParams& params);
    static std::vector<cv::Point> smoothContour(const std::vector<cv::Point>& contour,
                                                double smoothingMM, double pixelsPerMM,
                                                const ProcessingParams& params);
    static bool validateContour(const std::vector<cv::Point>& contour, const ProcessingParams& params);
    
    // Debug visualization methods
    static void saveDebugImage(const cv::Mat& image, const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithContours(const cv::Mat& image, const std::vector<std::vector<cv::Point>>& contours, 
                                          const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithBoundary(const cv::Mat& image, const std::vector<cv::Point>& boundary,
                                         const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithCleanContour(const cv::Mat& image, const std::vector<cv::Point>& contour,
                                              const std::string& filename, const ProcessingParams& params);
    
    // Legacy methods (for backward compatibility)
    static cv::Mat thresholdImage(const cv::Mat& img, int threshValue = 127);
    static std::vector<cv::Point> findLargestContour(const cv::Mat& binaryImg);
    static std::vector<cv::Point> approximatePolygon(const std::vector<cv::Point>& contour, 
                                                     double epsilonFactor = 0.02);
    static cv::Mat removeNoise(const cv::Mat& binaryImg, int kernelSize = 21, 
                              int blurSize = 101, int thresholdValue = 127);
    static std::vector<cv::Point> findMainContour(const cv::Mat& binaryImg);
    
    static std::vector<cv::Point> processImageToContour(const std::string& inputPath, 
                                                       const ProcessingParams& params);
    static std::vector<cv::Point> processImageToContour(const std::string& inputPath);
};

} // namespace OutlineTool
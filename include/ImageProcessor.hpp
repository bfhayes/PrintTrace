#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/photo.hpp>
#include <string>
#include <vector>

namespace PrintTrace {

class ImageProcessor {
public:
    struct ProcessingParams {
        // Lightbox dimensions after perspective correction
        int lightboxWidthPx = 3240;      // Target lightbox width in pixels
        int lightboxHeightPx = 3240;     // Target lightbox height in pixels
        double lightboxWidthMM = 162.0;  // Real-world lightbox width in millimeters
        double lightboxHeightMM = 162.0; // Real-world lightbox height in millimeters
        
        // Edge detection parameters
        double cannyLower = 50.0;
        double cannyUpper = 150.0;
        int cannyAperture = 3;

        // CLAHE parameters for lighting normalization
        double claheClipLimit = 2.0;
        int claheTileSize = 8;

        // Lab-based paper masking thresholds
        int labLThresh     = 150;        // L channel threshold (lowered to include shadows)
        int labAmin        = 110;        // A channel min (more inclusive)
        int labAmax        = 145;        // A channel max (more inclusive)
        int labBmin        = 110;        // B channel min (more inclusive)
        int labBmax        = 145;        // B channel max (more inclusive)

        // Grayscale fallback offset
        double otsuOffset  = 100.0;

        // Morphological & hole-fill parameters
        bool disableMorphology = false;   // Disable morphological operations
        int  morphKernelSize   = 5;       // Kernel size for basic morphology
        int  largeKernel       = 15;      // Kernel size for paper mask open/close
        double holeAreaRatio   = 0.001;   // Fill holes smaller than 0.1% of paper area
        int  inpaintRadius     = 5;       // Radius for inpainting
        uchar neutralGray      = 128;     // Gray fill for non-paper areas

        // Object detection parameters
        bool useAdaptiveThreshold = true;
        double manualThreshold    = 0.0;  // 0 = auto
        double thresholdOffset    = 0.0;  // Offset from auto threshold

        // Multi-contour detection parameters
        bool mergeNearbyContours    = true;
        double contourMergeDistanceMM = 5.0;

        // Contour filtering parameters
        double minContourArea  = 500.0;
        double minSolidity     = 0.3;
        double maxAspectRatio  = 20.0;

        // Polygon approximation
        double polygonEpsilonFactor = 0.005;  // Default ~1% for good balance
        bool forceConvex = false;            // Apply convex hull before simplification

        // Sub-pixel refinement
        bool enableSubPixelRefinement = true;
        int  cornerWinSize           = 5;
        int  cornerZeroZone          = -1;

        // Validation parameters
        bool validateClosedContour = true;
        double minPerimeter        = 100.0;

        // Tolerance/dilation for 3D printing
        double dilationAmountMM  = 0.0;

        // Smoothing for 3D printing
        bool enableSmoothing     = true;
        double smoothingAmountMM = 0.5;  // Increased for more smoothness
        int  smoothingMode       = 1;

        // Performance optimization
        bool enableInpainting    = false;  // Enable inpainting for paper isolation

        // Debug visualization  
        bool enableDebugOutput  = false;
        bool verboseOutput     = true;       // Enable verbose console output and debug images
        std::string debugOutputPath = "./debug/";
        
        // Debug image stack (for automatic numbering)
        mutable std::vector<std::pair<cv::Mat, std::string>> debugImageStack;
    };

    static cv::Mat loadImage(const std::string& path);
    static cv::Mat convertToGrayscale(const cv::Mat& img);
    
    // New streamlined corner detection pipeline methods
    static cv::Mat convertBGRToLab(const cv::Mat& bgrImg);
    static cv::Mat applyCLAHEToL(const cv::Mat& labImg, const ProcessingParams& params);
    static cv::Mat divisionNormalization(const cv::Mat& labImg);
    static cv::Mat buildPaperMask(const cv::Mat& labImg, const cv::Mat& normalizedL, const ProcessingParams& params);
    static cv::Mat morphologicalCleanup(const cv::Mat& mask, const ProcessingParams& params);
    static std::vector<cv::Point2f> detectCornersFromContour(const cv::Mat& mask, const ProcessingParams& params);
    static std::vector<cv::Point2f> detectCornersFromEdges(const cv::Mat& normalizedL, const ProcessingParams& params);
    static std::vector<cv::Point2f> orderCorners(const std::vector<cv::Point2f>& corners);
    static bool validateCorners(const std::vector<cv::Point2f>& corners, const cv::Size& imageSize, const ProcessingParams& params);
    static cv::Mat validateWarpedImage(const cv::Mat& warpedImg, const ProcessingParams& params);
    
    // Master streamlined corner detection function
    static std::vector<cv::Point2f> detectLightboxCorners(const cv::Mat& bgrImg, const ProcessingParams& params);
    
    static cv::Mat normalizeLighting(const cv::Mat& inputImg, const ProcessingParams& params);
    static cv::Mat detectEdges(const cv::Mat& normalizedImg, const cv::Mat& originalImg, const ProcessingParams& params);
    static cv::Mat detectLightboxBoundary(const cv::Mat& grayImg, const ProcessingParams& params);
    static std::vector<cv::Point> findBoundaryContour(const cv::Mat& edgeImg, const ProcessingParams& params);
    static std::vector<cv::Point2f> refineCorners(const std::vector<cv::Point>& corners,
                                                  const cv::Mat& grayImg,
                                                  const ProcessingParams& params);
    static std::pair<cv::Mat, double> warpImage(const cv::Mat& originalImg,
                                               const std::vector<cv::Point2f>& corners,
                                               const cv::Size& targetSize, double realWorldWidthMM, double realWorldHeightMM);
    static std::pair<cv::Mat, double> warpImage(const cv::Mat& binaryImg,
                                               const std::vector<cv::Point>& approx,
                                               int side, double realWorldSizeMM);
    static std::vector<cv::Point> findObjectContour(const cv::Mat& warpedImg, const ProcessingParams& params);
    static std::vector<cv::Point> mergeNearbyContours(const std::vector<std::vector<cv::Point>>& contours,
                                                      double mergeDistancePx, const ProcessingParams& params);
    static std::vector<cv::Point2f> refineContour(const std::vector<cv::Point>& contour,
                                                  const cv::Mat& grayImg,
                                                  const ProcessingParams& params);
    static std::vector<cv::Point> dilateContour(const std::vector<cv::Point>& contour,
                                                double dilationMM, double pixelsPerMM,
                                                const ProcessingParams& params);
    static std::vector<cv::Point> smoothContour(const std::vector<cv::Point>& contour,
                                                double smoothingMM, double pixelsPerMM,
                                                const ProcessingParams& params);
    static std::vector<cv::Point> smoothContourMorphological(const std::vector<cv::Point>& contour,
                                                             double smoothingMM, double pixelsPerMM,
                                                             const ProcessingParams& params);
    static std::vector<cv::Point> smoothContourCurvatureBased(const std::vector<cv::Point>& contour,
                                                              double smoothingMM, double pixelsPerMM,
                                                              const ProcessingParams& params);
    static bool validateContour(const std::vector<cv::Point>& contour, const ProcessingParams& params);
    static void saveDebugImage(const cv::Mat& image, const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithContours(const cv::Mat& image, const std::vector<std::vector<cv::Point>>& contours,
                                          const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithBoundary(const cv::Mat& image, const std::vector<cv::Point>& boundary,
                                           const std::string& filename, const ProcessingParams& params);
    static void saveDebugImageWithCleanContour(const cv::Mat& image, const std::vector<cv::Point>& contour,
                                               const std::string& filename, const ProcessingParams& params);
    
    // New debug stack methods
    static void pushDebugImage(const cv::Mat& image, const std::string& name, const ProcessingParams& params);
    static void pushDebugContour(const cv::Mat& image, const std::vector<cv::Point>& contour, 
                                 const std::string& name, const ProcessingParams& params);
    static void flushDebugStack(const ProcessingParams& params);
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

    static std::pair<cv::Mat, std::vector<cv::Point>> processImageToStage(
        const std::string& inputPath,
        const ProcessingParams& params,
        int target_stage
    );
};

} // namespace PrintTrace

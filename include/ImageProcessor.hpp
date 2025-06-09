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
        int thresholdValue = 127;
        int noiseKernelSize = 21;
        int blurSize = 101;
        double polygonEpsilonFactor = 0.02;
    };

    static cv::Mat loadImage(const std::string& path);
    static cv::Mat convertToGrayscale(const cv::Mat& img);
    static cv::Mat thresholdImage(const cv::Mat& img, int threshValue = 127);
    
    static std::vector<cv::Point> findLargestContour(const cv::Mat& binaryImg);
    static std::vector<cv::Point> approximatePolygon(const std::vector<cv::Point>& contour, 
                                                     double epsilonFactor = 0.02);
    
    static std::pair<cv::Mat, double> warpImage(const cv::Mat& binaryImg, 
                                               const std::vector<cv::Point>& approx,
                                               int side, double realWorldSizeMM);
    
    static cv::Mat removeNoise(const cv::Mat& binaryImg, 
                              int kernelSize = 21, 
                              int blurSize = 101,
                              int thresholdValue = 127);
    
    static std::vector<cv::Point> findMainContour(const cv::Mat& binaryImg);
    
    static std::vector<cv::Point> processImageToContour(const std::string& inputPath, 
                                                       const ProcessingParams& params);
    static std::vector<cv::Point> processImageToContour(const std::string& inputPath);
};

} // namespace OutlineTool
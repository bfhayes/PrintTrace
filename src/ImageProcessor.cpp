#include "ImageProcessor.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace cv;
using namespace std;

namespace OutlineTool {

Mat ImageProcessor::loadImage(const string& path) {
    if (path.empty()) {
        throw invalid_argument("Image path cannot be empty");
    }
    
    cout << "[INFO] Loading image from: " << path << endl;
    Mat img = imread(path);
    if (img.empty()) {
        cerr << "[ERROR] Could not load image from " << path << endl;
        cerr << "[ERROR] Please check that the file exists and is a valid image format" << endl;
        throw runtime_error("Failed to load image: " + path);
    }
    
    if (img.rows < 100 || img.cols < 100) {
        throw runtime_error("Image too small (minimum 100x100 pixels required)");
    }
    
    cout << "[INFO] Image loaded successfully. Shape: " << img.rows << " x " << img.cols << endl;
    return img;
}

Mat ImageProcessor::convertToGrayscale(const Mat& img) {
    cout << "[INFO] Converting image to grayscale." << endl;
    Mat gray;
    cvtColor(img, gray, COLOR_BGR2GRAY);
    return gray;
}

Mat ImageProcessor::thresholdImage(const Mat& img, int threshValue) {
    cout << "[INFO] Applying binary threshold with value: " << threshValue << endl;
    Mat binary;
    threshold(img, binary, threshValue, 255, THRESH_BINARY);
    return binary;
}

vector<Point> ImageProcessor::findLargestContour(const Mat& binaryImg) {
    cout << "[INFO] Finding contours in the binary image." << endl;
    vector<vector<Point>> contours;
    findContours(binaryImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        cerr << "[ERROR] No contours found in the image." << endl;
        throw runtime_error("No contours found in the image.");
    }

    double maxArea = 0.0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = static_cast<int>(i);
        }
    }
    
    cout << "[INFO] Found " << contours.size() << " contours; selecting the largest." << endl;
    return contours[maxIdx];
}

vector<Point> ImageProcessor::approximatePolygon(const vector<Point>& contour, double epsilonFactor) {
    cout << "[INFO] Approximating contour to polygon." << endl;
    double perimeter = arcLength(contour, true);
    double epsilon = epsilonFactor * perimeter;
    vector<Point> approx;
    approxPolyDP(contour, approx, epsilon, true);
    return approx;
}

pair<Mat, double> ImageProcessor::warpImage(const Mat& binaryImg, const vector<Point>& approx,
                                           int side, double realWorldSizeMM) {
    if (binaryImg.empty()) {
        throw invalid_argument("Input image is empty");
    }
    
    if (approx.size() != 4) {
        cerr << "[ERROR] Expected 4 corners, but got " << approx.size() << endl;
        cerr << "[ERROR] Could not detect a rectangular boundary in the image" << endl;
        throw runtime_error("Expected to find 4 corners in the contour.");
    }
    
    if (side <= 0 || realWorldSizeMM <= 0) {
        throw invalid_argument("Side length and real world size must be positive");
    }

    cout << "[INFO] Warping image to a square region." << endl;

    // Convert to Point2f for perspective transform
    vector<Point2f> corners(4);
    for (int i = 0; i < 4; i++) {
        corners[i] = Point2f(static_cast<float>(approx[i].x), static_cast<float>(approx[i].y));
    }

    // Order corners: top-left, top-right, bottom-right, bottom-left
    auto sumCompare = [](const Point2f& a, const Point2f& b) {
        return (a.x + a.y) < (b.x + b.y);
    };
    auto diffCompare = [](const Point2f& a, const Point2f& b) {
        return (a.y - a.x) < (b.y - b.x);
    };

    vector<Point2f> orderedCorners(4);
    auto minSum = min_element(corners.begin(), corners.end(), sumCompare);
    auto maxSum = max_element(corners.begin(), corners.end(), sumCompare);
    auto minDiff = min_element(corners.begin(), corners.end(), diffCompare);
    auto maxDiff = max_element(corners.begin(), corners.end(), diffCompare);

    orderedCorners[0] = *minSum;  // top-left
    orderedCorners[1] = *minDiff; // top-right
    orderedCorners[2] = *maxSum;  // bottom-right
    orderedCorners[3] = *maxDiff; // bottom-left

    double pixelsPerMM = static_cast<double>(side) / realWorldSizeMM;
    cout << "[INFO] Computed pixels per mm: " << pixelsPerMM << endl;

    vector<Point2f> dstPts{
        Point2f(0.0f, 0.0f), 
        Point2f(static_cast<float>(side - 1), 0.0f),
        Point2f(static_cast<float>(side - 1), static_cast<float>(side - 1)),
        Point2f(0.0f, static_cast<float>(side - 1))
    };

    Mat transformMatrix = getPerspectiveTransform(orderedCorners, dstPts);
    Mat warped;
    warpPerspective(binaryImg, warped, transformMatrix, Size(side, side));
    
    return make_pair(warped, pixelsPerMM);
}

Mat ImageProcessor::removeNoise(const Mat& binaryImg, int kernelSize, int blurSize, int thresholdValue) {
    cout << "[INFO] Removing noise using morphological operations." << endl;

    Mat inverted;
    bitwise_not(binaryImg, inverted);

    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat opened, closed, dilated, blurred;

    morphologyEx(inverted, opened, MORPH_OPEN, kernel);
    morphologyEx(opened, closed, MORPH_CLOSE, kernel);
    dilate(closed, dilated, kernel, Point(-1, -1), 4);
    GaussianBlur(dilated, blurred, Size(blurSize, blurSize), 0);

    Mat cleaned;
    threshold(blurred, cleaned, thresholdValue, 255, THRESH_BINARY);
    return cleaned;
}

vector<Point> ImageProcessor::findMainContour(const Mat& binaryImg) {
    cout << "[INFO] Finding the main (largest) contour in the cleaned image." << endl;
    vector<vector<Point>> contours;
    findContours(binaryImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        cerr << "[ERROR] No contours found for the object." << endl;
        throw runtime_error("No contours found for the object.");
    }

    double maxArea = 0.0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = static_cast<int>(i);
        }
    }
    return contours[maxIdx];
}

vector<Point> ImageProcessor::processImageToContour(const string& inputPath, const ProcessingParams& params) {
    cout << "[INFO] Starting image processing pipeline..." << endl;

    Mat img = loadImage(inputPath);
    Mat gray = convertToGrayscale(img);
    Mat binary = thresholdImage(gray, params.thresholdValue);

    vector<Point> largestContour = findLargestContour(binary);
    vector<Point> approxPoly = approximatePolygon(largestContour, params.polygonEpsilonFactor);

    auto [warped, pixelsPerMM] = warpImage(binary, approxPoly, params.warpSize, params.realWorldSizeMM);
    Mat cleaned = removeNoise(warped, params.noiseKernelSize, params.blurSize, params.thresholdValue);
    vector<Point> objectContour = findMainContour(cleaned);

    cout << "[INFO] Image processing pipeline completed successfully." << endl;
    return objectContour;
}

vector<Point> ImageProcessor::processImageToContour(const string& inputPath) {
    ProcessingParams params;
    return processImageToContour(inputPath, params);
}

} // namespace OutlineTool
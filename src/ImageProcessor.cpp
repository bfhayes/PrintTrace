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

// New improved pipeline methods

Mat ImageProcessor::normalizeLighting(const Mat& grayImg, const ProcessingParams& params) {
    cout << "[INFO] Normalizing lighting using CLAHE (Contrast Limited Adaptive Histogram Equalization)" << endl;
    
    auto clahe = createCLAHE();
    clahe->setClipLimit(params.claheClipLimit);
    clahe->setTilesGridSize(Size(params.claheTileSize, params.claheTileSize));
    
    Mat normalized;
    clahe->apply(grayImg, normalized);
    
    cout << "[INFO] Lighting normalization completed" << endl;
    return normalized;
}

Mat ImageProcessor::detectEdges(const Mat& grayImg, const ProcessingParams& params) {
    cout << "[INFO] Detecting edges for lightbox boundary" << endl;
    cout << "[INFO] Canny thresholds: " << params.cannyLower << " - " << params.cannyUpper << endl;
    
    Mat edges;
    Canny(grayImg, edges, params.cannyLower, params.cannyUpper, params.cannyAperture);
    
    cout << "[INFO] Edge detection completed" << endl;
    return edges;
}

// New method specifically for lightbox boundary detection
Mat ImageProcessor::detectLightboxBoundary(const Mat& grayImg, const ProcessingParams& params) {
    cout << "[INFO] Detecting lightbox boundary using intensity-based method" << endl;
    
    // For a backlit lightbox, we can use simple thresholding to find the bright square
    Mat binary;
    
    // Find a good threshold automatically using Otsu's method
    double otsu_thresh = threshold(grayImg, binary, 0, 255, THRESH_BINARY + THRESH_OTSU);
    cout << "[INFO] Otsu threshold for lightbox: " << otsu_thresh << endl;
    
    // The lightbox should be the brightest region, so threshold higher than Otsu
    double lightbox_thresh = otsu_thresh + (255 - otsu_thresh) * 0.3; // 30% into the bright range
    threshold(grayImg, binary, lightbox_thresh, 255, THRESH_BINARY);
    
    cout << "[INFO] Using lightbox threshold: " << lightbox_thresh << endl;
    
    // Save intermediate results for debugging
    saveDebugImage(binary, "04a_lightbox_binary.jpg", params);
    
    // Clean up the binary image to get clean rectangular boundary
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel); // Fill small gaps
    morphologyEx(binary, binary, MORPH_OPEN, kernel);  // Remove small noise
    
    saveDebugImage(binary, "04b_lightbox_cleaned.jpg", params);
    
    // Get edges of the cleaned binary image
    Mat edges;
    Canny(binary, edges, 50, 150, 3);
    
    cout << "[INFO] Lightbox boundary detection completed" << endl;
    return edges;
}

vector<Point> ImageProcessor::findBoundaryContour(const Mat& edgeImg, const ProcessingParams& params) {
    cout << "[INFO] Finding lightbox boundary contour" << endl;
    
    vector<vector<Point>> contours;
    findContours(edgeImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        throw runtime_error("No contours found in edge image");
    }
    
    cout << "[INFO] Found " << contours.size() << " contours for lightbox boundary" << endl;
    
    // For lightbox detection, we want the largest rectangular contour
    // Lightbox should be the largest bright region in the image
    vector<pair<int, double>> candidates;
    
    // Find largest contour by area (lightbox should be biggest bright region)
    double maxArea = 0;
    int bestIdx = -1;
    
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            bestIdx = static_cast<int>(i);
        }
    }
    
    if (bestIdx >= 0) {
        cout << "[INFO] Selected lightbox contour with area: " << maxArea << endl;
        return contours[bestIdx];
    }
    
    throw runtime_error("No suitable lightbox boundary found");
}

vector<Point2f> ImageProcessor::refineCorners(const vector<Point>& corners, 
                                             const Mat& grayImg, 
                                             const ProcessingParams& params) {
    if (!params.enableSubPixelRefinement || corners.size() != 4) {
        cout << "[INFO] Skipping sub-pixel refinement" << endl;
        vector<Point2f> result;
        for (const auto& pt : corners) {
            result.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }
        return result;
    }
    
    cout << "[INFO] Refining corners with sub-pixel accuracy" << endl;
    
    vector<Point2f> cornerFloat;
    for (const auto& pt : corners) {
        cornerFloat.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
    }
    
    TermCriteria criteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 0.1);
    cornerSubPix(grayImg, cornerFloat, 
                Size(params.cornerWinSize, params.cornerWinSize),
                Size(params.cornerZeroZone, params.cornerZeroZone),
                criteria);
    
    cout << "[INFO] Sub-pixel corner refinement completed" << endl;
    return cornerFloat;
}

vector<Point> ImageProcessor::findObjectContour(const Mat& warpedImg, const ProcessingParams& params) {
    cout << "[INFO] Finding object contour in perspective-corrected image using traditional method" << endl;
    
    // For object detection in warped image, use binary thresholding instead of edge detection
    // This works better because the object should be darker than the white lightbox background
    
    Mat binary;
    
    // Use Otsu thresholding to automatically separate object from background
    double otsu_thresh = threshold(warpedImg, binary, 0, 255, THRESH_BINARY_INV + THRESH_OTSU);
    cout << "[INFO] Object detection threshold: " << otsu_thresh << endl;
    
    saveDebugImage(binary, "08a_object_binary.jpg", params);
    
    // Clean up the binary image with morphological operations
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel); // Fill small gaps
    morphologyEx(binary, binary, MORPH_OPEN, kernel);  // Remove small noise
    
    saveDebugImage(binary, "08b_object_cleaned.jpg", params);
    
    // Find contours
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        throw runtime_error("No object contours found in warped image");
    }
    
    saveDebugImageWithContours(warpedImg, contours, "08c_all_contours.jpg", params);
    
    cout << "[INFO] Found " << contours.size() << " object contours" << endl;
    
    // Find the best object contour - should be largest and reasonably centered
    double maxArea = 0;
    int bestIdx = -1;
    
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        
        // Must have reasonable area
        if (area < 1000) continue;
        
        // Check if contour is roughly centered (object should be in center after warping)
        Moments m = moments(contours[i]);
        if (m.m00 > 0) {
            Point2f centroid(static_cast<float>(m.m10/m.m00), static_cast<float>(m.m01/m.m00));
            Point2f imageCenter(static_cast<float>(warpedImg.cols/2), static_cast<float>(warpedImg.rows/2));
            double distance = norm(centroid - imageCenter);
            double maxDistance = min(warpedImg.cols, warpedImg.rows) * 0.4; // 40% of image size
            
            if (distance < maxDistance && area > maxArea) {
                maxArea = area;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    
    if (bestIdx < 0) {
        // Fallback to largest contour
        cout << "[WARN] No well-centered contours found, using largest contour" << endl;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    
    if (bestIdx < 0) {
        throw runtime_error("No valid object contours found");
    }
    
    vector<Point> objectContour = contours[bestIdx];
    cout << "[INFO] Selected object contour with area: " << maxArea << endl;
    
    // Apply very conservative polygon approximation to preserve detail for CAD
    vector<Point> approx;
    double perimeter = arcLength(objectContour, true);
    
    // Use much smaller epsilon to preserve more detail points
    double epsilon = params.polygonEpsilonFactor * 0.25 * perimeter; // Very conservative
    approxPolyDP(objectContour, approx, epsilon, true);
    
    // Only apply approximation if we have too many points (>100) and it doesn't reduce below reasonable detail
    if (objectContour.size() > 100 && approx.size() >= objectContour.size() * 0.3) {
        cout << "[INFO] Applied conservative polygon approximation" << endl;
        cout << "[INFO] Object contour found with " << approx.size() << " vertices (from " << objectContour.size() << " original points)" << endl;
        return approx;
    } else {
        // For smaller contours or when approximation removes too much detail, use original contour
        cout << "[INFO] Preserving original contour detail (no approximation)" << endl;
        cout << "[INFO] Object contour found with " << objectContour.size() << " vertices (original detail preserved)" << endl;
        return objectContour;
    }
}

vector<Point2f> ImageProcessor::refineContour(const vector<Point>& contour,
                                             const Mat& grayImg,
                                             const ProcessingParams& params) {
    if (!params.enableSubPixelRefinement) {
        vector<Point2f> result;
        for (const auto& pt : contour) {
            result.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }
        return result;
    }
    
    cout << "[INFO] Refining contour with sub-pixel accuracy" << endl;
    
    vector<Point2f> contourFloat;
    for (const auto& pt : contour) {
        contourFloat.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
    }
    
    TermCriteria criteria(TermCriteria::EPS + TermCriteria::MAX_ITER, 30, 0.1);
    cornerSubPix(grayImg, contourFloat, 
                Size(params.cornerWinSize, params.cornerWinSize),
                Size(params.cornerZeroZone, params.cornerZeroZone),
                criteria);
    
    return contourFloat;
}

vector<Point> ImageProcessor::dilateContour(const vector<Point>& contour, 
                                           double dilationMM, double pixelsPerMM,
                                           const ProcessingParams& params) {
    if (dilationMM <= 0.0) {
        cout << "[INFO] No dilation requested, returning original contour" << endl;
        return contour;
    }
    
    cout << "[INFO] Dilating contour by " << dilationMM << "mm for 3D printing tolerance" << endl;
    
    // Convert dilation from mm to pixels
    double dilationPixels = dilationMM * pixelsPerMM;
    cout << "[INFO] Dilation in pixels: " << dilationPixels << endl;
    
    // Create a mask from the contour
    Rect boundingRect = cv::boundingRect(contour);
    
    // Add padding around bounding rect for dilation
    int padding = static_cast<int>(dilationPixels * 3); // Extra padding to avoid edge effects
    Size imageSize(boundingRect.width + 2 * padding, boundingRect.height + 2 * padding);
    Point offset(-boundingRect.x + padding, -boundingRect.y + padding);
    
    // Create binary mask
    Mat mask = Mat::zeros(imageSize, CV_8UC1);
    
    // Adjust contour points for the mask coordinate system
    vector<Point> adjustedContour;
    for (const Point& pt : contour) {
        adjustedContour.push_back(Point(pt.x + offset.x, pt.y + offset.y));
    }
    
    // Fill the contour in the mask
    vector<vector<Point>> contourVec = {adjustedContour};
    fillPoly(mask, contourVec, Scalar(255));
    
    saveDebugImage(mask, "09a_contour_mask.jpg", params);
    
    // Apply morphological dilation
    int kernelSize = static_cast<int>(dilationPixels * 2 + 1); // Ensure odd kernel size
    if (kernelSize < 3) kernelSize = 3;
    
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat dilated;
    dilate(mask, dilated, kernel);
    
    saveDebugImage(dilated, "09b_dilated_mask.jpg", params);
    
    // Extract the dilated contour
    vector<vector<Point>> dilatedContours;
    findContours(dilated, dilatedContours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (dilatedContours.empty()) {
        cout << "[WARN] No contours found after dilation, returning original" << endl;
        return contour;
    }
    
    // Find the largest contour (should be our dilated shape)
    double maxArea = 0;
    int bestIdx = -1;
    for (size_t i = 0; i < dilatedContours.size(); i++) {
        double area = contourArea(dilatedContours[i]);
        if (area > maxArea) {
            maxArea = area;
            bestIdx = static_cast<int>(i);
        }
    }
    
    if (bestIdx < 0) {
        cout << "[WARN] No valid dilated contour found, returning original" << endl;
        return contour;
    }
    
    // Convert back to original coordinate system
    vector<Point> finalContour;
    for (const Point& pt : dilatedContours[bestIdx]) {
        finalContour.push_back(Point(pt.x - offset.x, pt.y - offset.y));
    }
    
    cout << "[INFO] Dilation complete. Original: " << contour.size() << " points, Dilated: " << finalContour.size() << " points" << endl;
    return finalContour;
}

bool ImageProcessor::validateContour(const vector<Point>& contour, const ProcessingParams& params) {
    cout << "[INFO] Validating contour for CAD suitability" << endl;
    
    if (contour.size() < 3) {
        cout << "[ERROR] Contour has too few points: " << contour.size() << endl;
        return false;
    }
    
    // Check minimum perimeter
    double perimeter = arcLength(contour, true);
    if (perimeter < params.minPerimeter) {
        cout << "[ERROR] Contour perimeter too small: " << perimeter << " < " << params.minPerimeter << endl;
        return false;
    }
    
    // Check if contour is closed (if validation enabled)
    if (params.validateClosedContour) {
        Point first = contour.front();
        Point last = contour.back();
        double distance = norm(first - last);
        if (distance > 5.0) { // Allow small gap
            cout << "[WARN] Contour may not be properly closed, gap: " << distance << " pixels" << endl;
        }
    }
    
    // Check for self-intersections (basic check)
    if (contour.size() > 4) {
        for (size_t i = 0; i < contour.size() - 1; i++) {
            for (size_t j = i + 2; j < contour.size() - 1; j++) {
                if (j == contour.size() - 1 && i == 0) continue; // Skip adjacent segments
                
                // Simple intersection check would go here
                // For now, we'll skip this expensive check
            }
        }
    }
    
    cout << "[INFO] Contour validation passed" << endl;
    return true;
}

// Debug visualization methods

void ImageProcessor::saveDebugImage(const Mat& image, const string& filename, const ProcessingParams& params) {
    if (!params.enableDebugOutput) return;
    
    string fullPath = params.debugOutputPath + filename;
    imwrite(fullPath, image);
    cout << "[DEBUG] Saved debug image: " << fullPath << endl;
}

void ImageProcessor::saveDebugImageWithContours(const Mat& image, const vector<vector<Point>>& contours, 
                                               const string& filename, const ProcessingParams& params) {
    if (!params.enableDebugOutput) return;
    
    Mat debugImg;
    if (image.channels() == 1) {
        cvtColor(image, debugImg, COLOR_GRAY2BGR);
    } else {
        debugImg = image.clone();
    }
    
    // Draw all contours in different colors
    for (size_t i = 0; i < contours.size(); i++) {
        Scalar color(rand() % 256, rand() % 256, rand() % 256);
        drawContours(debugImg, contours, static_cast<int>(i), color, 2);
        
        // Add contour index
        if (!contours[i].empty()) {
            Moments m = moments(contours[i]);
            if (m.m00 > 0) {
                Point centroid(static_cast<int>(m.m10/m.m00), static_cast<int>(m.m01/m.m00));
                putText(debugImg, to_string(i), centroid, FONT_HERSHEY_SIMPLEX, 0.8, color, 2);
            }
        }
    }
    
    string fullPath = params.debugOutputPath + filename;
    imwrite(fullPath, debugImg);
    cout << "[DEBUG] Saved contour debug image: " << fullPath << " (" << contours.size() << " contours)" << endl;
}

void ImageProcessor::saveDebugImageWithBoundary(const Mat& image, const vector<Point>& boundary,
                                               const string& filename, const ProcessingParams& params) {
    if (!params.enableDebugOutput) return;
    
    Mat debugImg;
    if (image.channels() == 1) {
        cvtColor(image, debugImg, COLOR_GRAY2BGR);
    } else {
        debugImg = image.clone();
    }
    
    // Draw boundary contour in bright green
    vector<vector<Point>> boundaryVec = {boundary};
    drawContours(debugImg, boundaryVec, 0, Scalar(0, 255, 0), 3);
    
    // Draw corner points in red circles
    for (size_t i = 0; i < boundary.size(); i++) {
        circle(debugImg, boundary[i], 8, Scalar(0, 0, 255), -1);
        putText(debugImg, to_string(i), 
                Point(boundary[i].x + 10, boundary[i].y - 10), 
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 2);
    }
    
    string fullPath = params.debugOutputPath + filename;
    imwrite(fullPath, debugImg);
    cout << "[DEBUG] Saved boundary debug image: " << fullPath << " (" << boundary.size() << " points)" << endl;
}

void ImageProcessor::saveDebugImageWithCleanContour(const Mat& image, const vector<Point>& contour,
                                                   const string& filename, const ProcessingParams& params) {
    if (!params.enableDebugOutput) return;
    
    Mat debugImg;
    if (image.channels() == 1) {
        cvtColor(image, debugImg, COLOR_GRAY2BGR);
    } else {
        debugImg = image.clone();
    }
    
    // Draw just a clean outline in bright green, no labels or points
    vector<vector<Point>> contourVec = {contour};
    drawContours(debugImg, contourVec, 0, Scalar(0, 255, 0), 3); // Bright green, 3px thick
    
    string fullPath = params.debugOutputPath + filename;
    imwrite(fullPath, debugImg);
    cout << "[DEBUG] Saved clean contour debug image: " << fullPath << " (" << contour.size() << " points)" << endl;
}

// Legacy methods (for backward compatibility)

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

// New improved warpImage that works with Point2f
pair<Mat, double> ImageProcessor::warpImage(const Mat& originalImg, const vector<Point2f>& corners,
                                           int side, double realWorldSizeMM) {
    if (originalImg.empty()) {
        throw invalid_argument("Input image is empty");
    }
    
    if (corners.size() != 4) {
        cerr << "[ERROR] Expected 4 corners, but got " << corners.size() << endl;
        cerr << "[ERROR] Could not detect a rectangular boundary in the image" << endl;
        throw runtime_error("Expected to find 4 corners in the contour.");
    }
    
    if (side <= 0 || realWorldSizeMM <= 0) {
        throw invalid_argument("Side length and real world size must be positive");
    }

    cout << "[INFO] Warping image to a square region using refined corners." << endl;

    // Order corners: top-left, top-right, bottom-right, bottom-left
    auto sumCompare = [](const Point2f& a, const Point2f& b) {
        return (a.x + a.y) < (b.x + b.y);
    };
    auto diffCompare = [](const Point2f& a, const Point2f& b) {
        return (a.y - a.x) < (b.y - b.x);
    };

    vector<Point2f> cornersCopy = corners;
    vector<Point2f> orderedCorners(4);
    auto minSum = min_element(cornersCopy.begin(), cornersCopy.end(), sumCompare);
    auto maxSum = max_element(cornersCopy.begin(), cornersCopy.end(), sumCompare);
    auto minDiff = min_element(cornersCopy.begin(), cornersCopy.end(), diffCompare);
    auto maxDiff = max_element(cornersCopy.begin(), cornersCopy.end(), diffCompare);

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
    warpPerspective(originalImg, warped, transformMatrix, Size(side, side));
    
    cout << "[INFO] Perspective correction completed" << endl;
    return make_pair(warped, pixelsPerMM);
}

// Legacy warpImage method for backward compatibility
pair<Mat, double> ImageProcessor::warpImage(const Mat& binaryImg, const vector<Point>& approx,
                                           int side, double realWorldSizeMM) {
    vector<Point2f> corners;
    for (const auto& pt : approx) {
        corners.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
    }
    return warpImage(binaryImg, corners, side, realWorldSizeMM);
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
    cout << "[INFO] Starting improved CAD-optimized image processing pipeline..." << endl;

    // Load and prepare image
    Mat img = loadImage(inputPath);
    Mat gray = convertToGrayscale(img);
    
    // Create debug output directory
    if (params.enableDebugOutput) {
        // Create debug directory
        system(("mkdir -p " + params.debugOutputPath).c_str());
        cout << "[DEBUG] Debug output enabled, saving to: " << params.debugOutputPath << endl;
    }
    
    // Save original images
    saveDebugImage(img, "01_original.jpg", params);
    saveDebugImage(gray, "02_grayscale.jpg", params);
    
    // Step 1: Normalize lighting using CLAHE
    Mat normalized = normalizeLighting(gray, params);
    saveDebugImage(normalized, "03_normalized.jpg", params);
    
    // Step 2: Detect lightbox boundary (specialized for backlit white square)
    Mat lightboxEdges = detectLightboxBoundary(normalized, params);
    saveDebugImage(lightboxEdges, "04_lightbox_edges.jpg", params);
    
    // Step 3: Find boundary contour for perspective correction
    vector<Point> boundaryContour = findBoundaryContour(lightboxEdges, params);
    saveDebugImageWithBoundary(gray, boundaryContour, "05_boundary_contour.jpg", params);
    
    // Use more aggressive approximation for boundary detection (need exactly 4 corners)
    vector<Point> approxBoundary;
    double epsilon_factor = 0.02; // Start with 2% for boundary detection
    int max_attempts = 10;
    
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        approxBoundary = approximatePolygon(boundaryContour, epsilon_factor);
        cout << "[INFO] Boundary approximation attempt " << (attempt + 1) 
             << " with epsilon " << epsilon_factor << " -> " << approxBoundary.size() << " vertices" << endl;
        
        if (approxBoundary.size() == 4) {
            break; // Found 4 corners!
        } else if (approxBoundary.size() > 4) {
            epsilon_factor += 0.01; // Increase epsilon to get fewer vertices
        } else {
            epsilon_factor -= 0.005; // Decrease epsilon to get more vertices
            if (epsilon_factor <= 0.005) break; // Don't go too low
        }
    }
    
    // If we still don't have 4 corners, try a different approach
    if (approxBoundary.size() != 4) {
        cout << "[WARN] Could not find 4-corner boundary, attempting convex hull approach..." << endl;
        vector<Point> hull;
        convexHull(boundaryContour, hull);
        approxBoundary = approximatePolygon(hull, 0.02);
        cout << "[INFO] Convex hull approach resulted in " << approxBoundary.size() << " vertices" << endl;
        
        // Final fallback: use bounding rectangle
        if (approxBoundary.size() != 4) {
            cout << "[WARN] Using bounding rectangle as final fallback..." << endl;
            Rect boundingRect = cv::boundingRect(boundaryContour);
            approxBoundary = {
                Point(boundingRect.x, boundingRect.y),
                Point(boundingRect.x + boundingRect.width, boundingRect.y),
                Point(boundingRect.x + boundingRect.width, boundingRect.y + boundingRect.height),
                Point(boundingRect.x, boundingRect.y + boundingRect.height)
            };
            cout << "[INFO] Bounding rectangle created with 4 corners" << endl;
        }
    }
    
    // Save boundary approximation results
    saveDebugImageWithBoundary(gray, approxBoundary, "06_boundary_approximated.jpg", params);
    
    // Step 4: Refine corners with sub-pixel accuracy
    vector<Point2f> refinedCorners = refineCorners(approxBoundary, normalized, params);
    
    // Step 5: Warp perspective using the original grayscale image (not binary!)
    auto [warped, pixelsPerMM] = warpImage(normalized, refinedCorners, params.warpSize, params.realWorldSizeMM);
    saveDebugImage(warped, "07_warped.jpg", params);
    
    // Step 6: Find object contour in warped image
    vector<Point> objectContour = findObjectContour(warped, params);
    saveDebugImageWithCleanContour(warped, objectContour, "08_object_contour.jpg", params);
    
    // Step 7: Apply dilation for 3D printing tolerance if requested
    if (params.dilationAmountMM > 0.0) {
        objectContour = dilateContour(objectContour, params.dilationAmountMM, pixelsPerMM, params);
        saveDebugImageWithCleanContour(warped, objectContour, "09_dilated_contour.jpg", params);
    }
    
    // Step 8: Validate the contour for CAD suitability
    if (!validateContour(objectContour, params)) {
        throw runtime_error("Generated contour failed validation checks");
    }
    
    cout << "[INFO] CAD-optimized image processing pipeline completed successfully." << endl;
    cout << "[INFO] Final contour has " << objectContour.size() << " points" << endl;
    return objectContour;
}

vector<Point> ImageProcessor::processImageToContour(const string& inputPath) {
    ProcessingParams params;
    return processImageToContour(inputPath, params);
}

} // namespace OutlineTool
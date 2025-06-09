#include "ImageProcessor.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

using namespace cv;
using namespace std;

namespace PrintTrace {

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
    cout << "[INFO] Finding object contour in perspective-corrected image with enhanced thresholding" << endl;
    
    Mat binary;
    double final_threshold;
    
    if (params.useAdaptiveThreshold) {
        cout << "[INFO] Using adaptive thresholding for object detection" << endl;
        adaptiveThreshold(warpedImg, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 21, 10);
        final_threshold = -1; // Adaptive doesn't have single threshold
        saveDebugImage(binary, "08a_object_binary_adaptive.jpg", params);
    } else if (params.manualThreshold > 0.0) {
        cout << "[INFO] Using manual threshold: " << params.manualThreshold << endl;
        final_threshold = params.manualThreshold;
        threshold(warpedImg, binary, final_threshold, 255, THRESH_BINARY_INV);
        saveDebugImage(binary, "08a_object_binary_manual.jpg", params);
    } else {
        // Use Otsu with optional offset
        double otsu_thresh = threshold(warpedImg, binary, 0, 255, THRESH_BINARY_INV + THRESH_OTSU);
        final_threshold = otsu_thresh + params.thresholdOffset;
        
        cout << "[INFO] Otsu threshold: " << otsu_thresh;
        if (params.thresholdOffset != 0.0) {
            cout << ", with offset: " << params.thresholdOffset << ", final: " << final_threshold;
        }
        cout << endl;
        
        // Apply final threshold (may be different from Otsu if offset is used)
        if (params.thresholdOffset != 0.0) {
            threshold(warpedImg, binary, final_threshold, 255, THRESH_BINARY_INV);
        }
        
        saveDebugImage(binary, "08a_object_binary.jpg", params);
    }
    
    // Optionally clean up the binary image with morphological operations
    if (!params.disableMorphology) {
        cout << "[INFO] Applying morphological cleaning with kernel size: " << params.morphKernelSize << endl;
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(params.morphKernelSize, params.morphKernelSize));
        morphologyEx(binary, binary, MORPH_CLOSE, kernel); // Fill small gaps
        morphologyEx(binary, binary, MORPH_OPEN, kernel);  // Remove small noise
        saveDebugImage(binary, "08b_object_cleaned.jpg", params);
    } else {
        cout << "[INFO] Morphological cleaning disabled - preserving original binary image" << endl;
        saveDebugImage(binary, "08b_object_cleaned.jpg", params); // Save same as input for consistency
    }
    
    // Find contours
    vector<vector<Point>> contours;
    findContours(binary, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        throw runtime_error("No object contours found in warped image");
    }
    
    saveDebugImageWithContours(warpedImg, contours, "08c_all_contours.jpg", params);
    
    cout << "[INFO] Found " << contours.size() << " object contours" << endl;
    
    vector<Point> objectContour;
    
    if (params.mergeNearbyContours && contours.size() > 1) {
        cout << "[INFO] Attempting to merge nearby contours (max distance: " << params.contourMergeDistanceMM << "mm)" << endl;
        
        // Convert merge distance from mm to pixels
        double pixelsPerMMWidth = static_cast<double>(warpedImg.cols) / params.lightboxWidthMM;
        double pixelsPerMMHeight = static_cast<double>(warpedImg.rows) / params.lightboxHeightMM;
        // Use average for diagonal measurements
        double pixelsPerMM = (pixelsPerMMWidth + pixelsPerMMHeight) / 2.0;
        double mergeDistancePx = params.contourMergeDistanceMM * pixelsPerMM;
        
        objectContour = mergeNearbyContours(contours, mergeDistancePx, params);
        
        if (!objectContour.empty()) {
            cout << "[INFO] Successfully merged contours into single object outline" << endl;
        } else {
            cout << "[WARN] Contour merging failed, falling back to largest contour" << endl;
        }
    }
    
    // Fallback to single largest contour if merging failed or disabled
    if (objectContour.empty()) {
        cout << "[INFO] Using single largest contour approach" << endl;
        
        double maxArea = 0;
        int bestIdx = -1;
        
        for (size_t i = 0; i < contours.size(); i++) {
            double area = contourArea(contours[i]);
            
            // Must have reasonable area
            if (area < params.minContourArea) continue;
            
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
            // Fallback to largest contour regardless of position
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
        
        objectContour = contours[bestIdx];
        cout << "[INFO] Selected single contour with area: " << maxArea << endl;
    }
    
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

vector<Point> ImageProcessor::smoothContour(const vector<Point>& contour,
                                           double smoothingMM, double pixelsPerMM,
                                           const ProcessingParams& params) {
    if (smoothingMM <= 0.0 || !params.enableSmoothing) {
        cout << "[INFO] No smoothing requested, returning original contour" << endl;
        return contour;
    }
    
    cout << "[INFO] Smoothing contour by " << smoothingMM << "mm using " 
         << (params.smoothingMode == 0 ? "morphological" : "curvature-based") 
         << " method for easier 3D printing" << endl;
    
    if (params.smoothingMode == 0) {
        return smoothContourMorphological(contour, smoothingMM, pixelsPerMM, params);
    } else {
        return smoothContourCurvatureBased(contour, smoothingMM, pixelsPerMM, params);
    }
}

// New curvature-based smoothing method (default)
vector<Point> ImageProcessor::smoothContourCurvatureBased(const vector<Point>& contour,
                                                         double smoothingMM, double pixelsPerMM,
                                                         const ProcessingParams& params) {
    cout << "[INFO] Using curvature-based smoothing (preserves detail better)" << endl;
    
    // Convert smoothing from mm to pixels
    double smoothingPixels = smoothingMM * pixelsPerMM;
    cout << "[INFO] Smoothing in pixels: " << smoothingPixels << endl;
    
    // Method 1: Douglas-Peucker simplification to remove unnecessary points
    // This removes points that don't contribute significantly to the shape
    vector<Point> simplified;
    double epsilon = smoothingPixels * 0.5; // Simplification tolerance
    approxPolyDP(contour, simplified, epsilon, true);
    cout << "[INFO] Simplified from " << contour.size() << " to " << simplified.size() << " points" << endl;
    
    // Method 2: Local weighted averaging for smoothing sharp corners
    // This smooths kinks while preserving overall shape
    vector<Point> smoothed;
    int windowSize = static_cast<int>(smoothingPixels) | 1; // Make odd
    if (windowSize < 3) windowSize = 3;
    int halfWindow = windowSize / 2;
    
    for (size_t i = 0; i < simplified.size(); i++) {
        // Calculate local curvature to determine smoothing strength
        Point prev = simplified[(i - 1 + simplified.size()) % simplified.size()];
        Point curr = simplified[i];
        Point next = simplified[(i + 1) % simplified.size()];
        
        // Compute angle at current point
        Point v1 = prev - curr;
        Point v2 = next - curr;
        double angle = acos(v1.dot(v2) / (norm(v1) * norm(v2) + 1e-6));
        
        // Only smooth sharp corners (angle < 150 degrees)
        if (angle < CV_PI * 5.0 / 6.0) {
            // Weighted average of nearby points
            Point2f avgPoint(0, 0);
            float totalWeight = 0;
            
            for (int j = -halfWindow; j <= halfWindow; j++) {
                int idx = (i + j + simplified.size()) % simplified.size();
                float weight = 1.0f / (1.0f + abs(j)); // Distance-based weight
                avgPoint.x += simplified[idx].x * weight;
                avgPoint.y += simplified[idx].y * weight;
                totalWeight += weight;
            }
            
            avgPoint.x /= totalWeight;
            avgPoint.y /= totalWeight;
            
            // Blend based on curvature (sharper corners get more smoothing)
            float blendFactor = static_cast<float>((CV_PI - angle) / CV_PI);
            blendFactor = pow(blendFactor, 2.0f); // Non-linear blending
            
            Point smoothedPt;
            smoothedPt.x = static_cast<int>(curr.x * (1 - blendFactor) + avgPoint.x * blendFactor);
            smoothedPt.y = static_cast<int>(curr.y * (1 - blendFactor) + avgPoint.y * blendFactor);
            smoothed.push_back(smoothedPt);
        } else {
            // Keep straight sections unchanged
            smoothed.push_back(curr);
        }
    }
    
    // Method 3: Optional final polygon approximation for cleaner result
    vector<Point> finalContour;
    double finalEpsilon = smoothingPixels * 0.2; // Gentle final approximation
    approxPolyDP(smoothed, finalContour, finalEpsilon, true);
    
    // Debug visualization
    if (params.enableDebugOutput) {
        // Create visualization showing original vs smoothed
        Rect boundingRect = cv::boundingRect(contour);
        int padding = 20;
        Size imageSize(boundingRect.width + 2 * padding, boundingRect.height + 2 * padding);
        Point offset(-boundingRect.x + padding, -boundingRect.y + padding);
        
        Mat vis = Mat::zeros(imageSize, CV_8UC3);
        
        // Draw original in red
        vector<Point> origVis;
        for (const Point& pt : contour) {
            origVis.push_back(Point(pt.x + offset.x, pt.y + offset.y));
        }
        polylines(vis, vector<vector<Point>>{origVis}, true, Scalar(0, 0, 255), 2);
        
        // Draw smoothed in green
        vector<Point> smoothVis;
        for (const Point& pt : finalContour) {
            smoothVis.push_back(Point(pt.x + offset.x, pt.y + offset.y));
        }
        polylines(vis, vector<vector<Point>>{smoothVis}, true, Scalar(0, 255, 0), 2);
        
        saveDebugImage(vis, "10_smoothing_comparison.jpg", params);
    }
    
    cout << "[INFO] Curvature-based smoothing complete. Original: " << contour.size() << " points, Smoothed: " << finalContour.size() << " points" << endl;
    return finalContour;
}

// Legacy morphological smoothing method
vector<Point> ImageProcessor::smoothContourMorphological(const vector<Point>& contour,
                                                        double smoothingMM, double pixelsPerMM,
                                                        const ProcessingParams& params) {
    cout << "[INFO] Using morphological smoothing (legacy method, affects entire shape)" << endl;
    
    // Convert smoothing from mm to pixels
    double smoothingPixels = smoothingMM * pixelsPerMM;
    cout << "[INFO] Smoothing in pixels: " << smoothingPixels << endl;
    
    // Create a mask from the contour
    Rect boundingRect = cv::boundingRect(contour);
    
    // Add padding around bounding rect for morphological operations
    int padding = static_cast<int>(smoothingPixels * 3);
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
    
    saveDebugImage(mask, "10a_morph_smooth_mask.jpg", params);
    
    // Apply morphological operations for smoothing
    int kernelSize = static_cast<int>(smoothingPixels * 2 + 1);
    if (kernelSize < 3) kernelSize = 3;
    
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat smoothed;
    
    // Apply closing (dilation followed by erosion) to smooth out small indentations
    morphologyEx(mask, smoothed, MORPH_CLOSE, kernel);
    
    // Apply opening (erosion followed by dilation) to smooth out small protrusions
    morphologyEx(smoothed, smoothed, MORPH_OPEN, kernel);
    
    saveDebugImage(smoothed, "10b_morph_smoothed_mask.jpg", params);
    
    // Extract the smoothed contour
    vector<vector<Point>> smoothedContours;
    findContours(smoothed, smoothedContours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (smoothedContours.empty()) {
        cout << "[WARN] No contours found after morphological smoothing, returning original" << endl;
        return contour;
    }
    
    // Find the largest contour (should be our smoothed shape)
    double maxArea = 0;
    int bestIdx = -1;
    for (size_t i = 0; i < smoothedContours.size(); i++) {
        double area = contourArea(smoothedContours[i]);
        if (area > maxArea) {
            maxArea = area;
            bestIdx = static_cast<int>(i);
        }
    }
    
    if (bestIdx < 0) {
        cout << "[WARN] No valid smoothed contour found, returning original" << endl;
        return contour;
    }
    
    // Convert back to original coordinate system
    vector<Point> finalContour;
    for (const Point& pt : smoothedContours[bestIdx]) {
        finalContour.push_back(Point(pt.x - offset.x, pt.y - offset.y));
    }
    
    cout << "[INFO] Morphological smoothing complete. Original: " << contour.size() << " points, Smoothed: " << finalContour.size() << " points" << endl;
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
    
    // Create debug directory if it doesn't exist
    string createDirCommand = "mkdir -p \"" + params.debugOutputPath + "\"";
    system(createDirCommand.c_str());
    
    string fullPath = params.debugOutputPath + filename;
    bool success = imwrite(fullPath, image);
    if (success) {
        cout << "[DEBUG] Saved debug image: " << fullPath << endl;
    } else {
        cout << "[WARNING] Failed to save debug image: " << fullPath << endl;
    }
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
    
    // Create debug directory if it doesn't exist
    string createDirCommand = "mkdir -p \"" + params.debugOutputPath + "\"";
    system(createDirCommand.c_str());
    
    string fullPath = params.debugOutputPath + filename;
    bool success = imwrite(fullPath, debugImg);
    if (success) {
        cout << "[DEBUG] Saved contour debug image: " << fullPath << " (" << contours.size() << " contours)" << endl;
    } else {
        cout << "[WARNING] Failed to save contour debug image: " << fullPath << endl;
    }
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
    
    // Create debug directory if it doesn't exist
    string createDirCommand = "mkdir -p \"" + params.debugOutputPath + "\"";
    system(createDirCommand.c_str());
    
    string fullPath = params.debugOutputPath + filename;
    bool success = imwrite(fullPath, debugImg);
    if (success) {
        cout << "[DEBUG] Saved boundary debug image: " << fullPath << " (" << boundary.size() << " points)" << endl;
    } else {
        cout << "[WARNING] Failed to save boundary debug image: " << fullPath << endl;
    }
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
    
    // Create debug directory if it doesn't exist
    string createDirCommand = "mkdir -p \"" + params.debugOutputPath + "\"";
    system(createDirCommand.c_str());
    
    string fullPath = params.debugOutputPath + filename;
    bool success = imwrite(fullPath, debugImg);
    if (success) {
        cout << "[DEBUG] Saved clean contour debug image: " << fullPath << " (" << contour.size() << " points)" << endl;
    } else {
        cout << "[WARNING] Failed to save clean contour debug image: " << fullPath << endl;
    }
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

// New improved warpImage that works with Point2f and rectangular lightboxes
pair<Mat, double> ImageProcessor::warpImage(const Mat& originalImg, const vector<Point2f>& corners,
                                           const cv::Size& targetSize, double realWorldWidthMM, double realWorldHeightMM) {
    if (originalImg.empty()) {
        throw invalid_argument("Input image is empty");
    }
    
    if (corners.size() != 4) {
        cerr << "[ERROR] Expected 4 corners, but got " << corners.size() << endl;
        cerr << "[ERROR] Could not detect a rectangular boundary in the image" << endl;
        throw runtime_error("Expected to find 4 corners in the contour.");
    }
    
    if (targetSize.width <= 0 || targetSize.height <= 0 || realWorldWidthMM <= 0 || realWorldHeightMM <= 0) {
        throw invalid_argument("Target size and real world dimensions must be positive");
    }

    cout << "[INFO] Warping image to " << targetSize.width << "x" << targetSize.height << " region using refined corners." << endl;

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

    double pixelsPerMMWidth = static_cast<double>(targetSize.width) / realWorldWidthMM;
    double pixelsPerMMHeight = static_cast<double>(targetSize.height) / realWorldHeightMM;
    // Return average pixels per mm for backward compatibility
    double pixelsPerMM = (pixelsPerMMWidth + pixelsPerMMHeight) / 2.0;
    cout << "[INFO] Computed pixels per mm - Width: " << pixelsPerMMWidth << ", Height: " << pixelsPerMMHeight << ", Average: " << pixelsPerMM << endl;

    vector<Point2f> dstPts{
        Point2f(0.0f, 0.0f), 
        Point2f(static_cast<float>(targetSize.width - 1), 0.0f),
        Point2f(static_cast<float>(targetSize.width - 1), static_cast<float>(targetSize.height - 1)),
        Point2f(0.0f, static_cast<float>(targetSize.height - 1))
    };

    Mat transformMatrix = getPerspectiveTransform(orderedCorners, dstPts);
    Mat warped;
    warpPerspective(originalImg, warped, transformMatrix, targetSize);
    
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
    return warpImage(binaryImg, corners, cv::Size(side, side), realWorldSizeMM, realWorldSizeMM);
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
    auto [warped, pixelsPerMM] = warpImage(normalized, refinedCorners, 
                                          cv::Size(params.lightboxWidthPx, params.lightboxHeightPx), 
                                          params.lightboxWidthMM, params.lightboxHeightMM);
    saveDebugImage(warped, "07_warped.jpg", params);
    
    // Step 6: Find object contour in warped image
    vector<Point> objectContour = findObjectContour(warped, params);
    saveDebugImageWithCleanContour(warped, objectContour, "08_object_contour.jpg", params);
    
    // Step 7: Apply smoothing for easier 3D printing if requested
    if (params.enableSmoothing) {
        objectContour = smoothContour(objectContour, params.smoothingAmountMM, pixelsPerMM, params);
        saveDebugImageWithCleanContour(warped, objectContour, "09_smoothed_contour.jpg", params);
    }
    
    // Step 8: Apply dilation for 3D printing tolerance if requested
    if (params.dilationAmountMM > 0.0) {
        objectContour = dilateContour(objectContour, params.dilationAmountMM, pixelsPerMM, params);
        string filename = params.enableSmoothing ? "10_dilated_contour.jpg" : "09_dilated_contour.jpg";
        saveDebugImageWithCleanContour(warped, objectContour, filename, params);
    }
    
    // Step 9: Validate the contour for CAD suitability
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

std::pair<cv::Mat, std::vector<cv::Point>> ImageProcessor::processImageToStage(
    const std::string& inputPath, 
    const ProcessingParams& params,
    int target_stage
) {
    cout << "[INFO] Processing image to stage " << target_stage << endl;
    
    // Stage 0: Load and convert to grayscale
    Mat originalImg = loadImage(inputPath);
    Mat grayImg = convertToGrayscale(originalImg);
    
    // Save debug image for original
    saveDebugImage(originalImg, "00_original.jpg", params);
    saveDebugImage(grayImg, "01_grayscale.jpg", params);
    
    if (target_stage == 0) { // PRINT_TRACE_STAGE_LOADED
        return {grayImg.clone(), {}};
    }
    
    // Stage 1: Process to lightbox cropped (perspective correction)
    // First we need to detect the lightbox boundary
    Mat normalizedImg = normalizeLighting(grayImg, params);
    saveDebugImage(normalizedImg, "02_normalized.jpg", params);
    
    Mat boundaryEdges = detectLightboxBoundary(normalizedImg, params);
    saveDebugImage(boundaryEdges, "03_boundary_edges.jpg", params);
    
    vector<Point> boundaryContour = findBoundaryContour(boundaryEdges, params);
    saveDebugImageWithBoundary(boundaryEdges, boundaryContour, "04_boundary_contour.jpg", params);
    
    // Approximate to 4 corners with iterative refinement
    vector<Point> corners;
    bool found4Corners = false;
    double epsilonFactor = 0.02;
    
    for (int attempts = 0; attempts < 10 && !found4Corners; attempts++) {
        corners = approximatePolygon(boundaryContour, epsilonFactor);
        if (corners.size() == 4) {
            found4Corners = true;
            cout << "[INFO] Found 4 corners with epsilon factor: " << epsilonFactor << endl;
        } else {
            epsilonFactor += 0.005;
            cout << "[INFO] Attempt " << (attempts + 1) << ": Found " << corners.size() 
                 << " corners, trying epsilon: " << epsilonFactor << endl;
        }
    }
    
    if (!found4Corners) {
        cout << "[WARN] Could not find exactly 4 corners, trying fallback methods" << endl;
        
        // Fallback 1: Use convex hull
        vector<Point> hull;
        convexHull(boundaryContour, hull);
        corners = approximatePolygon(hull, 0.02);
        
        if (corners.size() != 4) {
            // Fallback 2: Use bounding rectangle corners
            Rect boundingRect = cv::boundingRect(boundaryContour);
            corners = {
                Point(boundingRect.x, boundingRect.y),
                Point(boundingRect.x + boundingRect.width, boundingRect.y),
                Point(boundingRect.x + boundingRect.width, boundingRect.y + boundingRect.height),
                Point(boundingRect.x, boundingRect.y + boundingRect.height)
            };
            cout << "[INFO] Using bounding rectangle as fallback" << endl;
        } else {
            cout << "[INFO] Convex hull fallback successful" << endl;
        }
    }
    
    // Refine corners with sub-pixel accuracy
    vector<Point2f> refinedCorners = refineCorners(corners, normalizedImg, params);
    
    // Warp image using the original grayscale (not binary) for better quality
    auto [warpedImg, pixelsPerMM] = warpImage(grayImg, refinedCorners, 
                                             cv::Size(params.lightboxWidthPx, params.lightboxHeightPx),
                                             params.lightboxWidthMM, params.lightboxHeightMM);
    
    saveDebugImage(warpedImg, "05_perspective_corrected.jpg", params);
    
    if (target_stage == 1) { // PRINT_TRACE_STAGE_LIGHTBOX_CROPPED
        return {warpedImg.clone(), {}};
    }
    
    // Stage 2: Normalized (already done above, just return warped + normalized)
    Mat warpedNormalized = normalizeLighting(warpedImg, params);
    saveDebugImage(warpedNormalized, "06_warped_normalized.jpg", params);
    
    if (target_stage == 2) { // PRINT_TRACE_STAGE_NORMALIZED
        return {warpedNormalized.clone(), {}};
    }
    
    // Stage 3: Boundary detected (return warped image with boundary data)
    if (target_stage == 3) { // PRINT_TRACE_STAGE_BOUNDARY_DETECTED
        // Convert corners back to vector<Point> for consistency
        vector<Point> cornerPoints;
        for (const auto& pt : refinedCorners) {
            cornerPoints.emplace_back(static_cast<int>(pt.x), static_cast<int>(pt.y));
        }
        return {warpedImg.clone(), cornerPoints};
    }
    
    // Stage 4: Object detected
    vector<Point> objectContour = findObjectContour(warpedImg, params);
    saveDebugImageWithCleanContour(warpedImg, objectContour, "07_object_contour.jpg", params);
    
    if (target_stage == 4) { // PRINT_TRACE_STAGE_OBJECT_DETECTED
        return {warpedImg.clone(), objectContour};
    }
    
    // Stage 5: Smoothed (if enabled)
    vector<Point> processedContour = objectContour;
    if (params.enableSmoothing) {
        processedContour = smoothContour(processedContour, params.smoothingAmountMM, pixelsPerMM, params);
        saveDebugImageWithCleanContour(warpedImg, processedContour, "08_smoothed_contour.jpg", params);
    }
    
    if (target_stage == 5) { // PRINT_TRACE_STAGE_SMOOTHED
        return {warpedImg.clone(), processedContour};
    }
    
    // Stage 6: Dilated (if enabled)
    if (params.dilationAmountMM > 0.0) {
        processedContour = dilateContour(processedContour, params.dilationAmountMM, pixelsPerMM, params);
        saveDebugImageWithCleanContour(warpedImg, processedContour, "09_dilated_contour.jpg", params);
    }
    
    if (target_stage == 6) { // PRINT_TRACE_STAGE_DILATED
        return {warpedImg.clone(), processedContour};
    }
    
    // Stage 7: Final (validate contour)
    if (!validateContour(processedContour, params)) {
        throw runtime_error("Final contour validation failed");
    }
    
    saveDebugImageWithCleanContour(warpedImg, processedContour, "10_final_contour.jpg", params);
    
    return {warpedImg.clone(), processedContour};
}

vector<Point> ImageProcessor::mergeNearbyContours(const vector<vector<Point>>& contours,
                                                 double mergeDistancePx, const ProcessingParams& params) {
    if (contours.empty()) return {};
    
    cout << "[INFO] Merging " << contours.size() << " contours with max distance: " << mergeDistancePx << "px" << endl;
    
    // Filter contours by minimum area first
    vector<vector<Point>> validContours;
    for (const auto& contour : contours) {
        double area = contourArea(contour);
        if (area >= params.minContourArea * 0.1) { // Use lower threshold for parts
            validContours.push_back(contour);
        }
    }
    
    if (validContours.empty()) return {};
    if (validContours.size() == 1) return validContours[0];
    
    cout << "[INFO] Found " << validContours.size() << " valid contours to merge" << endl;
    
    // Create a mask to draw all contours
    Mat mask = Mat::zeros(params.lightboxHeightPx, params.lightboxWidthPx, CV_8UC1);
    
    // Draw all valid contours on the mask
    for (size_t i = 0; i < validContours.size(); i++) {
        drawContours(mask, validContours, static_cast<int>(i), Scalar(255), FILLED);
    }
    
    // Apply morphological closing to connect nearby parts
    if (mergeDistancePx > 0) {
        int kernelSize = static_cast<int>(mergeDistancePx * 2);
        if (kernelSize > 0 && kernelSize % 2 == 0) kernelSize++; // Ensure odd size
        if (kernelSize < 3) kernelSize = 3;
        if (kernelSize > 21) kernelSize = 21; // Reasonable upper limit
        
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
        morphologyEx(mask, mask, MORPH_CLOSE, kernel);
        
        cout << "[INFO] Applied morphological closing with kernel size: " << kernelSize << endl;
    }
    
    saveDebugImage(mask, "08d_merged_mask.jpg", params);
    
    // Find contours on the merged mask
    vector<vector<Point>> mergedContours;
    findContours(mask, mergedContours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (mergedContours.empty()) {
        cout << "[WARN] No contours found after merging" << endl;
        return {};
    }
    
    // Find the largest merged contour
    double maxArea = 0;
    int bestIdx = -1;
    for (size_t i = 0; i < mergedContours.size(); i++) {
        double area = contourArea(mergedContours[i]);
        if (area > maxArea) {
            maxArea = area;
            bestIdx = static_cast<int>(i);
        }
    }
    
    if (bestIdx >= 0) {
        cout << "[INFO] Merged contour has area: " << maxArea << " (from " << mergedContours.size() << " merged contours)" << endl;
        return mergedContours[bestIdx];
    }
    
    return {};
}

} // namespace PrintTrace
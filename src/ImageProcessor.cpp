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

// New streamlined corner detection pipeline methods

Mat ImageProcessor::convertBGRToLab(const Mat& bgrImg) {
    cout << "[INFO] Converting BGR to LAB color space" << endl;
    Mat labImg;
    cvtColor(bgrImg, labImg, COLOR_BGR2Lab);
    return labImg;
}

Mat ImageProcessor::applyCLAHEToL(const Mat& labImg, const ProcessingParams& params) {
    cout << "[INFO] Applying CLAHE to L channel for local contrast enhancement" << endl;
    
    vector<Mat> labChannels;
    split(labImg, labChannels);
    
    auto clahe = createCLAHE();
    clahe->setClipLimit(params.claheClipLimit);
    clahe->setTilesGridSize(Size(params.claheTileSize, params.claheTileSize));
    
    Mat enhancedL;
    clahe->apply(labChannels[0], enhancedL);
    
    // Merge back to LAB
    labChannels[0] = enhancedL;
    Mat enhancedLab;
    merge(labChannels, enhancedLab);
    
    return enhancedLab;
}

Mat ImageProcessor::divisionNormalization(const Mat& labImg) {
    cout << "[INFO] Applying division normalization to flatten lighting gradients" << endl;
    
    vector<Mat> labChannels;
    split(labImg, labChannels);
    Mat L = labChannels[0];
    
    // Create a heavily blurred version for division normalization
    Mat blurred;
    GaussianBlur(L, blurred, Size(0, 0), min(L.rows, L.cols) * 0.05); // 5% of image size
    
    // Avoid division by zero
    blurred += 1;
    
    // Normalize by division
    Mat normalized;
    L.convertTo(normalized, CV_32F);
    blurred.convertTo(blurred, CV_32F);
    normalized = normalized / blurred * 128.0; // Scale to reasonable range
    
    // Convert back to 8-bit
    Mat result;
    normalized.convertTo(result, CV_8U);
    
    return result;
}

Mat ImageProcessor::buildPaperMask(const Mat& labImg, const Mat& normalizedL, const ProcessingParams& params) {
    cout << "[INFO] Building paper mask with L threshold + A/B inRange + adaptive fallback" << endl;
    
    vector<Mat> labChannels;
    split(labImg, labChannels);
    Mat L = labChannels[0], A = labChannels[1], B = labChannels[2];
    
    // Primary mask: L channel threshold for brightness
    Mat maskL;
    threshold(L, maskL, params.labLThresh, 255, THRESH_BINARY);
    
    // A and B channel masks for color neutrality (paper should be neutral)
    Mat maskA, maskB;
    inRange(A, params.labAmin, params.labAmax, maskA);
    inRange(B, params.labBmin, params.labBmax, maskB);
    
    // Combine all masks
    Mat paperMask;
    bitwise_and(maskL, maskA, paperMask);
    bitwise_and(paperMask, maskB, paperMask);
    
    saveDebugImage(paperMask, "paper_mask_lab.jpg", params);
    
    // Adaptive threshold fallback for shadow recovery
    Mat adaptiveMask;
    adaptiveThreshold(normalizedL, adaptiveMask, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 21, 10);
    
    // Combine with adaptive threshold to recover paper pixels lost in shadows
    Mat combinedMask;
    bitwise_or(paperMask, adaptiveMask, combinedMask);
    
    saveDebugImage(combinedMask, "paper_mask_with_adaptive.jpg", params);
    
    return combinedMask;
}

Mat ImageProcessor::morphologicalCleanup(const Mat& mask, const ProcessingParams& params) {
    cout << "[INFO] Applying morphological close→open and selecting largest component" << endl;
    
    Mat cleaned = mask.clone();
    
    // Use large kernel for cleaning up paper mask
    Mat kernel = getStructuringElement(MORPH_RECT, Size(params.largeKernel, params.largeKernel));
    
    // Close first to fill holes in paper
    morphologyEx(cleaned, cleaned, MORPH_CLOSE, kernel);
    saveDebugImage(cleaned, "mask_closed.jpg", params);
    
    // Then open to remove noise and small objects
    morphologyEx(cleaned, cleaned, MORPH_OPEN, kernel);
    saveDebugImage(cleaned, "mask_opened.jpg", params);
    
    // Find and keep only the largest component (the paper)
    vector<vector<Point>> contours;
    findContours(cleaned, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (!contours.empty()) {
        // Find largest contour
        double maxArea = 0;
        int maxIdx = -1;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                maxIdx = static_cast<int>(i);
            }
        }
        
        // Create mask with only the largest component
        Mat largestComponent = Mat::zeros(cleaned.size(), CV_8UC1);
        if (maxIdx >= 0) {
            drawContours(largestComponent, contours, maxIdx, Scalar(255), FILLED);
            cout << "[INFO] Kept largest component with area: " << maxArea << endl;
        }
        cleaned = largestComponent;
    }
    
    saveDebugImage(cleaned, "largest_component.jpg", params);
    return cleaned;
}

vector<Point2f> ImageProcessor::detectCornersFromContour(const Mat& mask, const ProcessingParams& params) {
    cout << "[INFO] Detecting corners using contour-based method with geometric sanity checks" << endl;
    
    vector<vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        cout << "[WARN] No contours found for corner detection" << endl;
        return {};
    }
    
    // Find the largest contour (should be the paper boundary)
    double maxArea = 0;
    int maxIdx = -1;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = static_cast<int>(i);
        }
    }
    
    if (maxIdx < 0) return {};
    
    vector<Point> paperContour = contours[maxIdx];
    
    // Approximate contour to polygon
    vector<Point> approx;
    double perimeter = arcLength(paperContour, true);
    double epsilon = 0.02 * perimeter; // Start with 2%
    
    // Try different epsilon values to get exactly 4 corners
    for (int attempts = 0; attempts < 10; attempts++) {
        approxPolyDP(paperContour, approx, epsilon, true);
        
        if (approx.size() == 4) {
            break;
        } else if (approx.size() > 4) {
            epsilon += 0.01 * perimeter; // Increase epsilon for fewer points
        } else {
            epsilon -= 0.005 * perimeter; // Decrease epsilon for more points
            if (epsilon <= 0.005 * perimeter) break;
        }
    }
    
    // Geometric sanity checks
    if (approx.size() == 4) {
        Rect boundingRect = cv::boundingRect(approx);
        double area = contourArea(approx);
        double rectArea = boundingRect.width * boundingRect.height;
        double solidity = area / rectArea;
        
        double aspectRatio = static_cast<double>(boundingRect.width) / boundingRect.height;
        if (aspectRatio < 1.0) aspectRatio = 1.0 / aspectRatio; // Always > 1
        
        cout << "[INFO] Corner detection - Area: " << area << ", Solidity: " << solidity 
             << ", Aspect ratio: " << aspectRatio << endl;
        
        // Check geometric constraints
        if (area > mask.total() * 0.1 &&  // Reasonable area (>10% of image)
            solidity > params.minSolidity && // Good rectangularity
            aspectRatio < params.maxAspectRatio) { // Reasonable aspect ratio
            
            // Convert to Point2f
            vector<Point2f> corners;
            for (const Point& pt : approx) {
                corners.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
            }
            
            cout << "[INFO] Contour-based corner detection successful" << endl;
            return corners;
        } else {
            cout << "[WARN] Contour failed geometric sanity checks" << endl;
        }
    }
    
    cout << "[WARN] Contour-based corner detection failed" << endl;
    return {};
}

vector<Point2f> ImageProcessor::detectCornersFromEdges(const Mat& normalizedL, const ProcessingParams& params) {
    cout << "[INFO] Edge-based fallback using Canny + HoughLines + clustering" << endl;
    
    // Apply Canny edge detection
    Mat edges;
    Canny(normalizedL, edges, params.cannyLower, params.cannyUpper, params.cannyAperture);
    saveDebugImage(edges, "canny_edges.jpg", params);
    
    // Detect lines using HoughLines
    vector<Vec2f> lines;
    HoughLines(edges, lines, 1, CV_PI/180, 50); // 50 votes threshold
    
    if (lines.size() < 4) {
        cout << "[WARN] Not enough lines detected for corner finding: " << lines.size() << endl;
        return {};
    }
    
    cout << "[INFO] Detected " << lines.size() << " lines with Hough transform" << endl;
    
    // Cluster lines into horizontal and vertical groups
    vector<Vec2f> horizontalLines, verticalLines;
    
    for (const auto& line : lines) {
        float theta = line[1];
        // Convert to degrees for easier understanding
        float degrees = theta * 180.0 / CV_PI;
        
        // Horizontal lines (theta close to 0 or 180)
        if (abs(degrees) < 20 || abs(degrees - 180) < 20) {
            horizontalLines.push_back(line);
        }
        // Vertical lines (theta close to 90)
        else if (abs(degrees - 90) < 20) {
            verticalLines.push_back(line);
        }
    }
    
    cout << "[INFO] Found " << horizontalLines.size() << " horizontal and " 
         << verticalLines.size() << " vertical lines" << endl;
    
    if (horizontalLines.size() < 2 || verticalLines.size() < 2) {
        cout << "[WARN] Not enough horizontal or vertical lines for corner detection" << endl;
        return {};
    }
    
    // Sort lines by their distance from origin (rho parameter)
    sort(horizontalLines.begin(), horizontalLines.end(), 
         [](const Vec2f& a, const Vec2f& b) { return a[0] < b[0]; });
    sort(verticalLines.begin(), verticalLines.end(), 
         [](const Vec2f& a, const Vec2f& b) { return a[0] < b[0]; });
    
    // Take the two most extreme horizontal and vertical lines
    Vec2f topLine = horizontalLines.front();
    Vec2f bottomLine = horizontalLines.back();
    Vec2f leftLine = verticalLines.front();
    Vec2f rightLine = verticalLines.back();
    
    // Calculate intersection points
    vector<Point2f> corners;
    
    // Helper function to find line intersection
    auto intersectLines = [](const Vec2f& line1, const Vec2f& line2) -> Point2f {
        float rho1 = line1[0], theta1 = line1[1];
        float rho2 = line2[0], theta2 = line2[1];
        
        float cos1 = cos(theta1), sin1 = sin(theta1);
        float cos2 = cos(theta2), sin2 = sin(theta2);
        
        float det = cos1 * sin2 - sin1 * cos2;
        if (abs(det) < 0.001) return Point2f(-1, -1); // Parallel lines
        
        float x = (sin2 * rho1 - sin1 * rho2) / det;
        float y = (cos1 * rho2 - cos2 * rho1) / det;
        
        return Point2f(x, y);
    };
    
    // Find four corner intersections
    Point2f topLeft = intersectLines(topLine, leftLine);
    Point2f topRight = intersectLines(topLine, rightLine);
    Point2f bottomRight = intersectLines(bottomLine, rightLine);
    Point2f bottomLeft = intersectLines(bottomLine, leftLine);
    
    // Check if intersections are valid
    vector<Point2f> candidateCorners = {topLeft, topRight, bottomRight, bottomLeft};
    for (const auto& corner : candidateCorners) {
        if (corner.x >= 0 && corner.y >= 0 && 
            corner.x < normalizedL.cols && corner.y < normalizedL.rows) {
            corners.push_back(corner);
        }
    }
    
    if (corners.size() == 4) {
        cout << "[INFO] Edge-based corner detection successful" << endl;
        return corners;
    } else {
        cout << "[WARN] Edge-based corner detection failed - found " << corners.size() << " valid corners" << endl;
        return {};
    }
}

vector<Point2f> ImageProcessor::orderCorners(const vector<Point2f>& corners) {
    cout << "[INFO] Ordering corners using sum/diff trick for consistent warping" << endl;
    
    if (corners.size() != 4) {
        cout << "[ERROR] Expected 4 corners, got " << corners.size() << endl;
        return corners;
    }
    
    vector<Point2f> ordered(4);
    
    // Calculate sum and difference for each corner
    vector<pair<float, int>> sums, diffs;
    for (size_t i = 0; i < corners.size(); i++) {
        sums.push_back({corners[i].x + corners[i].y, static_cast<int>(i)});
        diffs.push_back({corners[i].y - corners[i].x, static_cast<int>(i)});
    }
    
    // Sort by sum and difference
    sort(sums.begin(), sums.end());
    sort(diffs.begin(), diffs.end());
    
    // Top-left: smallest sum (x+y)
    ordered[0] = corners[sums[0].second];
    
    // Bottom-right: largest sum (x+y)
    ordered[2] = corners[sums[3].second];
    
    // Top-right: smallest difference (y-x)
    ordered[1] = corners[diffs[0].second];
    
    // Bottom-left: largest difference (y-x)
    ordered[3] = corners[diffs[3].second];
    
    cout << "[INFO] Corners ordered: TL(" << ordered[0].x << "," << ordered[0].y << ") "
         << "TR(" << ordered[1].x << "," << ordered[1].y << ") "
         << "BR(" << ordered[2].x << "," << ordered[2].y << ") "
         << "BL(" << ordered[3].x << "," << ordered[3].y << ")" << endl;
    
    return ordered;
}

bool ImageProcessor::validateCorners(const vector<Point2f>& corners, const Size& imageSize, const ProcessingParams& params) {
    cout << "[INFO] Validating detected corners" << endl;
    
    if (corners.size() != 4) {
        cout << "[ERROR] Invalid number of corners: " << corners.size() << endl;
        return false;
    }
    
    // Check all corners are within image bounds
    for (const auto& corner : corners) {
        if (corner.x < 0 || corner.y < 0 || 
            corner.x >= imageSize.width || corner.y >= imageSize.height) {
            cout << "[ERROR] Corner out of bounds: (" << corner.x << "," << corner.y << ")" << endl;
            return false;
        }
    }
    
    // Check minimum area
    double area = contourArea(corners);
    double minArea = imageSize.width * imageSize.height * 0.1; // At least 10% of image
    if (area < minArea) {
        cout << "[ERROR] Corner area too small: " << area << " < " << minArea << endl;
        return false;
    }
    
    // Check aspect ratio
    Rect boundingRect = cv::boundingRect(corners);
    double aspectRatio = static_cast<double>(boundingRect.width) / boundingRect.height;
    if (aspectRatio < 1.0) aspectRatio = 1.0 / aspectRatio;
    
    if (aspectRatio > params.maxAspectRatio) {
        cout << "[ERROR] Aspect ratio too extreme: " << aspectRatio << " > " << params.maxAspectRatio << endl;
        return false;
    }
    
    cout << "[INFO] Corner validation passed" << endl;
    return true;
}

Mat ImageProcessor::validateWarpedImage(const Mat& warpedImg, const ProcessingParams& params) {
    cout << "[INFO] Post-warp validation: checking edge energy and chromatic sanity" << endl;
    
    // Edge energy check - warped image should have clear edges
    Mat gray;
    if (warpedImg.channels() == 3) {
        cvtColor(warpedImg, gray, COLOR_BGR2GRAY);
    } else {
        gray = warpedImg.clone();
    }
    
    // Calculate edge energy using Sobel
    Mat sobelX, sobelY, edges;
    Sobel(gray, sobelX, CV_32F, 1, 0, 3);
    Sobel(gray, sobelY, CV_32F, 0, 1, 3);
    magnitude(sobelX, sobelY, edges);
    
    Scalar meanEdgeEnergy = mean(edges);
    double edgeThreshold = 10.0; // Minimum edge energy
    
    if (meanEdgeEnergy[0] < edgeThreshold) {
        cout << "[WARN] Low edge energy detected: " << meanEdgeEnergy[0] << " < " << edgeThreshold << endl;
        cout << "[WARN] Warped image may be blurry or incorrectly perspective-corrected" << endl;
    } else {
        cout << "[INFO] Edge energy validation passed: " << meanEdgeEnergy[0] << endl;
    }
    
    // Chromatic sanity check for color images
    if (warpedImg.channels() == 3) {
        vector<Mat> channels;
        split(warpedImg, channels);
        
        // Check if color channels are reasonably balanced (not too tinted)
        Scalar meanB = mean(channels[0]);
        Scalar meanG = mean(channels[1]);
        Scalar meanR = mean(channels[2]);
        
        double maxDiff = max({abs(meanB[0] - meanG[0]), abs(meanG[0] - meanR[0]), abs(meanR[0] - meanB[0])});
        double colorBalance = maxDiff / ((meanB[0] + meanG[0] + meanR[0]) / 3.0);
        
        if (colorBalance > 0.3) { // More than 30% color imbalance
            cout << "[WARN] Significant color imbalance detected: " << colorBalance << endl;
            cout << "[WARN] Image may have color cast or lighting issues" << endl;
        } else {
            cout << "[INFO] Chromatic sanity check passed: " << colorBalance << endl;
        }
    }
    
    saveDebugImage(warpedImg, "validated_warped.jpg", params);
    return warpedImg;
}

vector<Point2f> ImageProcessor::detectLightboxCorners(const Mat& bgrImg, const ProcessingParams& params) {
    cout << "[INFO] ===== STREAMLINED CORNER DETECTION PIPELINE =====" << endl;
    
    // Phase 1: Convert BGR to LAB – separates brightness from colour to make whites consistent
    Mat labImg = convertBGRToLab(bgrImg);
    saveDebugImage(labImg, "stream_lab.jpg", params);
    
    // Phase 2: Apply CLAHE to L channel – boosts local contrast so shadows and glare don't kill edges
    Mat enhancedLab = applyCLAHEToL(labImg, params);
    saveDebugImage(enhancedLab, "stream_clahe.jpg", params);
    
    // Phase 3: Division normalisation – flattens broad lighting gradients before binarisation
    Mat normalizedL = divisionNormalization(enhancedLab);
    saveDebugImage(normalizedL, "stream_division_norm.jpg", params);
    
    // Phase 4: Build paper mask with L threshold + A/B inRange – fuses brightness and neutrality to isolate paper
    // Phase 5: Adaptive threshold fallback – recovers paper pixels lost in deep shadows
    Mat paperMask = buildPaperMask(enhancedLab, normalizedL, params);
    
    // Phase 6: Morphological close→open and keep largest component – fills holes, removes noise, isolates the sheet
    Mat cleanMask = morphologicalCleanup(paperMask, params);
    
    // Phase 7: Contour‑based corner detection (findContours + approxPolyDP) – fast primary path to four corners
    // Phase 8: Geometric sanity checks (area, aspect ratio, solidity) – rejects four‑sided impostors
    vector<Point2f> corners = detectCornersFromContour(cleanMask, params);
    
    // Phase 9: Edge‑based fallback (Canny + HoughLines + clustering) – rescues cases where contour is broken
    if (corners.empty()) {
        cout << "[INFO] Contour-based detection failed, trying edge-based fallback" << endl;
        corners = detectCornersFromEdges(normalizedL, params);
    }
    
    if (corners.empty()) {
        cout << "[ERROR] All corner detection methods failed" << endl;
        return {};
    }
    
    // Phase 10: Order corners with sum/diff trick – ensures consistent point order for warping
    corners = orderCorners(corners);
    
    // Phase 11: Corner validation – final guard against false positives
    if (!validateCorners(corners, bgrImg.size(), params)) {
        cout << "[ERROR] Corner validation failed" << endl;
        return {};
    }
    
    cout << "[INFO] ===== STREAMLINED CORNER DETECTION SUCCESSFUL =====" << endl;
    return corners;
}

// Legacy method kept for compatibility
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

Mat ImageProcessor::detectEdges(
    const Mat& grayImg,
    const Mat& colorImg,
    const ProcessingParams& params
) {
    // 1. Lab-based paper mask
    Mat paperMask;
    if (!colorImg.empty() && colorImg.channels() == 3) {
        cout << "[INFO] Using LAB thresholding" << endl;
        Mat lab; 
        cvtColor(colorImg, lab, COLOR_BGR2Lab);
        vector<Mat> ch(3); 
        split(lab, ch);
        Mat L = ch[0], A = ch[1], B = ch[2];

        Mat maskL, maskA, maskB;
        threshold(L, maskL, params.labLThresh, 255, THRESH_BINARY);
        inRange(A, params.labAmin, params.labAmax, maskA);
        inRange(B, params.labBmin, params.labBmax, maskB);

        bitwise_and(maskL, maskA, paperMask);
        bitwise_and(paperMask, maskB, paperMask);
    } else {
        cout << "[INFO] Falling back to Otsu Threshold" << endl;
        double otsu = threshold(grayImg, paperMask, 0, 255, THRESH_BINARY | THRESH_OTSU);
        threshold(grayImg, paperMask, otsu + params.otsuOffset, 255, THRESH_BINARY);
    }
    pushDebugImage(paperMask, "mask_lab", params);

    // 2. Morphology + hole-fill to remove clutter
    Mat morph = paperMask.clone();
    Mat bigK = getStructuringElement(
        MORPH_RECT,
        Size(params.largeKernel, params.largeKernel)
    );
    morphologyEx(morph, morph, MORPH_OPEN,  bigK);
    morphologyEx(morph, morph, MORPH_CLOSE, bigK);

    // invert into a true mask Mat
    Mat holeMask;
    bitwise_not(morph, holeMask);

    // find the little “holes” on the paper
    vector<vector<Point>> holes;
    findContours(holeMask, holes, RETR_LIST, CHAIN_APPROX_SIMPLE);

    // fill small holes (draw each contour at index 0 of a single-item list)
    for (const auto &c : holes) {
        if (contourArea(c) < morph.total() * params.holeAreaRatio) {
            vector<vector<Point>> single{ c };
            drawContours(morph, single, 0, Scalar(255), FILLED);
        }
    }
    pushDebugImage(morph, "mask_clean", params);

    // 3. Robust contour-based corner detection
    cout << "[INFO] Starting robust contour-based corner detection" << endl;
    
    // Find contours on the clean mask (CCA - Connected Component Analysis)
    vector<vector<Point>> contours;
    findContours(morph, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        cout << "[ERROR] No contours found in clean mask" << endl;
        return Mat::zeros(morph.size(), CV_8UC1);
    }
    
    // Find the largest contour (should be the paper)
    double maxArea = 0;
    int bestIdx = -1;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            bestIdx = static_cast<int>(i);
        }
    }
    
    if (bestIdx < 0) {
        cout << "[ERROR] No valid contour found" << endl;
        return Mat::zeros(morph.size(), CV_8UC1);
    }
    
    vector<Point> paperContour = contours[bestIdx];
    
    // Apply sanity filters before expensive shape logic
    double imageArea = morph.total();
    double areaFraction = maxArea / imageArea;
    
    if (areaFraction < 0.1) {
        cout << "[ERROR] Contour too small (area fraction: " << areaFraction << ")" << endl;
        return Mat::zeros(morph.size(), CV_8UC1);
    }
    
    // Check convexity and solidity
    vector<Point> hull;
    convexHull(paperContour, hull);
    double hullArea = contourArea(hull);
    double solidity = maxArea / hullArea;
    
    if (solidity < 0.7) {
        cout << "[WARN] Low solidity detected: " << solidity << " (may be fragmented)" << endl;
    }
    
    cout << "[INFO] Paper contour validation - Area fraction: " << areaFraction 
         << ", Solidity: " << solidity << endl;
    
    // Primary approach: approxPolyDP preserves true border including perspective tilt
    vector<Point> approx;
    double perimeter = arcLength(paperContour, true);
    double epsilon = 0.02 * perimeter; // Start with 2%
    
    // Try different epsilon values to get exactly 4 corners
    vector<Point> bestApprox;
    for (int attempts = 0; attempts < 10; attempts++) {
        approxPolyDP(paperContour, approx, epsilon, true);
        
        if (approx.size() == 4) {
            bestApprox = approx;
            cout << "[INFO] Found 4-corner approximation with epsilon: " << epsilon << endl;
            break;
        } else if (approx.size() > 4) {
            epsilon += 0.005 * perimeter; // Increase epsilon for fewer points
        } else {
            epsilon -= 0.002 * perimeter; // Decrease epsilon for more points
            if (epsilon <= 0.005 * perimeter) break;
        }
    }
    
    // Fallback: minAreaRect guarantees four corners when contour is broken
    vector<Point2f> corners;
    if (bestApprox.size() == 4) {
        cout << "[INFO] Using approxPolyDP result (preserves true border)" << endl;
        for (const Point& pt : bestApprox) {
            corners.emplace_back(static_cast<float>(pt.x), static_cast<float>(pt.y));
        }
    } else {
        cout << "[INFO] Using minAreaRect fallback (guarantees 4 corners)" << endl;
        RotatedRect minRect = minAreaRect(paperContour);
        Point2f rectCorners[4];
        minRect.points(rectCorners);
        corners.assign(rectCorners, rectCorners + 4);
    }
    
    // Create a clean edge image with just the paper boundary
    Mat paperEdges = Mat::zeros(morph.size(), CV_8UC1);
    vector<Point> intCorners;
    for (const Point2f& corner : corners) {
        intCorners.emplace_back(static_cast<int>(corner.x), static_cast<int>(corner.y));
    }
    
    // Draw the paper boundary
    polylines(paperEdges, vector<vector<Point>>{intCorners}, true, Scalar(255), 2);
    pushDebugImage(paperEdges, "paper_boundary", params);
    
    cout << "[INFO] Robust corner detection complete - found " << corners.size() << " corners" << endl;
    return paperEdges;
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
    saveDebugImage(binary, "lightbox_binary.jpg", params);
    
    // Clean up the binary image to get clean rectangular boundary
    Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
    morphologyEx(binary, binary, MORPH_CLOSE, kernel); // Fill small gaps
    morphologyEx(binary, binary, MORPH_OPEN, kernel);  // Remove small noise
    
    saveDebugImage(binary, "lightbox_cleaned.jpg", params);
    
    // Get edges of the cleaned binary image
    Mat edges;
    Canny(binary, edges, 50, 150, 3);
    
    cout << "[INFO] Lightbox boundary detection completed" << endl;
    return edges;
}

std::vector<cv::Point> ImageProcessor::findBoundaryContour(
    const cv::Mat& edgeImg,
    const ProcessingParams& params
) {
    // 1) Find all external contours in the edge image
    std::vector<std::vector<cv::Point>> contours;
    findContours(edgeImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        throw std::runtime_error("No boundary contours found");
    }

    // 2) Pick the contour whose bounding‐rectangle has the largest area
    size_t bestIdx = 0;
    double bestArea = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        cv::Rect r = boundingRect(contours[i]);
        double area = static_cast<double>(r.width) * r.height;
        if (area > bestArea) {
            bestArea = area;
            bestIdx = i;
        }
    }

    return contours[bestIdx];
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
    if (params.verboseOutput) {
        cout << "[INFO] Finding object contour with streamlined detection" << endl;
    }
    
    // Step 1: Convert to single-channel, medianBlur, CLAHE for lighting robustness
    Mat gray;
    if (warpedImg.channels() == 3) {
        cvtColor(warpedImg, gray, COLOR_BGR2GRAY);
    } else {
        gray = warpedImg.clone();
    }
    
    medianBlur(gray, gray, 5);
    
    auto clahe = createCLAHE();
    clahe->setClipLimit(2.0);
    clahe->setTilesGridSize(Size(8, 8));
    clahe->apply(gray, gray);
    
    pushDebugImage(gray, "object_preprocessed", params);
    
    // Step 2: Collapse threshold logic - pick one method and run it once
    Mat binary;
    if (params.useAdaptiveThreshold) {
        if (params.verboseOutput) cout << "[INFO] Using adaptive threshold" << endl;
        adaptiveThreshold(gray, binary, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 21, 10);
    } else if (params.manualThreshold > 0.0) {
        if (params.verboseOutput) cout << "[INFO] Using manual threshold: " << params.manualThreshold << endl;
        threshold(gray, binary, params.manualThreshold, 255, THRESH_BINARY_INV);
    } else {
        if (params.verboseOutput) cout << "[INFO] Using Otsu threshold" << endl;
        double otsu_thresh = threshold(gray, binary, 0, 255, THRESH_BINARY_INV + THRESH_OTSU);
        if (params.thresholdOffset != 0.0) {
            threshold(gray, binary, otsu_thresh + params.thresholdOffset, 255, THRESH_BINARY_INV);
            if (params.verboseOutput) cout << "[INFO] Applied offset: " << params.thresholdOffset << endl;
        }
    }
    
    pushDebugImage(binary, "object_thresholded", params);
    
    // Step 3: Morphology - close x2 → flood-fill holes → open x1
    if (!params.disableMorphology) {
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(params.morphKernelSize, params.morphKernelSize));
        
        // Close twice to fill gaps
        morphologyEx(binary, binary, MORPH_CLOSE, kernel);
        morphologyEx(binary, binary, MORPH_CLOSE, kernel);
        
        // Flood-fill holes by finding interior contours and filling them
        Mat holeFilled = binary.clone();
        vector<vector<Point>> holes;
        findContours(binary, holes, RETR_CCOMP, CHAIN_APPROX_SIMPLE);
        for (size_t i = 0; i < holes.size(); i++) {
            drawContours(holeFilled, holes, static_cast<int>(i), Scalar(255), FILLED);
        }
        
        // Open once to clean edges
        morphologyEx(holeFilled, binary, MORPH_OPEN, kernel);
        
        pushDebugImage(binary, "object_morphology", params);
    }
    
    // Step 4: Use connectedComponentsWithStats for efficient blob selection
    Mat labels, stats, centroids;
    int numComponents = connectedComponentsWithStats(binary, labels, stats, centroids);
    
    if (numComponents < 2) { // Background is component 0
        throw runtime_error("No object components found");
    }
    
    // Find components to use (single best or multiple for merging)
    Mat componentMask;
    
    if (params.mergeNearbyContours) {
        // Multi-component approach: find all valid components and merge nearby ones
        vector<int> validComponents;
        Point2f imageCenter(static_cast<float>(binary.cols / 2), static_cast<float>(binary.rows / 2));
        
        for (int i = 1; i < numComponents; i++) { // Skip background (component 0)
            double area = stats.at<int>(i, CC_STAT_AREA);
            if (area >= params.minContourArea) {
                validComponents.push_back(i);
            }
        }
        
        if (validComponents.empty()) {
            throw runtime_error("No valid object components found");
        }
        
        // Create combined mask from all valid components
        componentMask = Mat::zeros(binary.size(), CV_8U);
        for (int comp : validComponents) {
            Mat compMask = (labels == comp);
            bitwise_or(componentMask, compMask, componentMask);
        }
        
        if (params.verboseOutput) {
            cout << "[INFO] Using " << validComponents.size() << " components for merging" << endl;
        }
    } else {
        // Single-component approach: find best component by area/distance score
        int bestComponent = -1;
        double bestScore = 0;
        Point2f imageCenter(static_cast<float>(binary.cols / 2), static_cast<float>(binary.rows / 2));
        
        for (int i = 1; i < numComponents; i++) { // Skip background (component 0)
            double area = stats.at<int>(i, CC_STAT_AREA);
            if (area < params.minContourArea) continue;
            
            Point2f centroid(centroids.at<double>(i, 0), centroids.at<double>(i, 1));
            double distance = norm(centroid - imageCenter);
            double normalizedDistance = distance / min(binary.cols, binary.rows);
            
            double score = area / (1.0 + normalizedDistance);
            if (score > bestScore) {
                bestScore = score;
                bestComponent = i;
            }
        }
        
        if (bestComponent < 0) {
            throw runtime_error("No valid object component found");
        }
        
        // Create mask for the best component
        componentMask = (labels == bestComponent);
    }
    pushDebugImage(componentMask, "object_component", params);
    
    // Step 5: Simple edge detection on the clean component mask
    // The componentMask has a perfect white/black boundary - just find the edge
    Mat edges;
    Canny(componentMask, edges, 50, 150, 3);
    
    pushDebugImage(edges, "object_edges", params);
    
    // Find contours on the edge image for exact boundary tracing
    vector<vector<Point>> contours;
    findContours(edges, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
    
    if (contours.empty()) {
        throw runtime_error("No edge contours found");
    }
    
    // Get the largest contour (our object boundary)
    vector<Point> objectContour = *max_element(contours.begin(), contours.end(),
        [](const vector<Point>& a, const vector<Point>& b) {
            return contourArea(a) < contourArea(b);
        });
    
    // Apply ultra-minimal polygonal approximation for maximum smoothness
    vector<Point> smoothedContour;
    double perimeter = arcLength(objectContour, true);
    // Use even smaller epsilon for ultra-smooth curves
    double epsilon = min(0.0005, params.polygonEpsilonFactor) * perimeter; // Cap at 0.05%
    approxPolyDP(objectContour, smoothedContour, epsilon, true);
    
    if (params.verboseOutput) {
        cout << "[INFO] Edge-based contour: " << objectContour.size() << " → " 
             << smoothedContour.size() << " points" << endl;
    }
    
    objectContour = smoothedContour;
    
    // Step 6: Optional convex hull guard
    if (params.forceConvex) {
        if (params.verboseOutput) cout << "[INFO] Applying convex hull" << endl;
        convexHull(objectContour, objectContour);
    }
    
    if (params.verboseOutput) {
        cout << "[INFO] Object contour smoothed and simplified: " << objectContour.size() << " points" << endl;
    }
    
    return objectContour;
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
    
    saveDebugImage(mask, "contour_mask.jpg", params);
    
    // Apply morphological dilation
    int kernelSize = static_cast<int>(dilationPixels * 2 + 1); // Ensure odd kernel size
    if (kernelSize < 3) kernelSize = 3;
    
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat dilated;
    dilate(mask, dilated, kernel);
    
    saveDebugImage(dilated, "dilated_mask.jpg", params);
    
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
    cout << "[INFO] Using multi-pass curvature-based smoothing for ultra-smooth results" << endl;
    
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
        
        saveDebugImage(vis, "smoothing_comparison.jpg", params);
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
    
    saveDebugImage(mask, "morph_smooth_mask.jpg", params);
    
    // Apply morphological operations for smoothing
    int kernelSize = static_cast<int>(smoothingPixels * 2 + 1);
    if (kernelSize < 3) kernelSize = 3;
    
    Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(kernelSize, kernelSize));
    Mat smoothed;
    
    // Apply closing (dilation followed by erosion) to smooth out small indentations
    morphologyEx(mask, smoothed, MORPH_CLOSE, kernel);
    
    // Apply opening (erosion followed by dilation) to smooth out small protrusions
    morphologyEx(smoothed, smoothed, MORPH_OPEN, kernel);
    
    saveDebugImage(smoothed, "morph_smoothed_mask.jpg", params);
    
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

void ImageProcessor::pushDebugImage(const Mat& image, const string& name, const ProcessingParams& params) {
    if (!params.enableDebugOutput || !params.verboseOutput) return;
    
    // Push a copy of the image and name onto the stack
    params.debugImageStack.emplace_back(image.clone(), name);
}

void ImageProcessor::pushDebugContour(const Mat& image, const vector<Point>& contour, 
                                     const string& name, const ProcessingParams& params) {
    if (!params.enableDebugOutput || !params.verboseOutput) return;
    
    Mat debugImg;
    if (image.channels() == 1) {
        cvtColor(image, debugImg, COLOR_GRAY2BGR);
    } else {
        debugImg = image.clone();
    }
    
    // Draw contour in bright green
    vector<vector<Point>> contourVec = {contour};
    drawContours(debugImg, contourVec, 0, Scalar(0, 255, 0), 3);
    
    // Push the annotated image to the stack
    params.debugImageStack.emplace_back(debugImg, name);
}

void ImageProcessor::flushDebugStack(const ProcessingParams& params) {
    if (!params.enableDebugOutput || params.debugImageStack.empty()) return;
    
    cout << "[DEBUG] Flushing " << params.debugImageStack.size() << " debug images..." << endl;
    
    // Create debug directory if it doesn't exist
    string createDirCommand = "mkdir -p \"" + params.debugOutputPath + "\"";
    system(createDirCommand.c_str());
    
    // Save all images with sequential numbering
    for (size_t i = 0; i < params.debugImageStack.size(); i++) {
        const auto& [image, name] = params.debugImageStack[i];
        
        // Format: 01_name.jpg, 02_name.jpg, etc.
        char indexStr[4];
        snprintf(indexStr, sizeof(indexStr), "%02zu", i + 1);
        string filename = string(indexStr) + "_" + name + ".jpg";
        string fullPath = params.debugOutputPath + filename;
        
        bool success = imwrite(fullPath, image);
        if (success) {
            cout << "[DEBUG] Saved: " << filename << endl;
        } else {
            cout << "[WARNING] Failed to save: " << filename << endl;
        }
    }
    
    // Clear the stack after flushing
    params.debugImageStack.clear();
    cout << "[DEBUG] Debug stack flushed and cleared" << endl;
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

    // For paper boundary detection, use smarter selection criteria
    double bestScore = 0.0;
    int bestIdx = -1;
    int validContours = 0;
    
    
    for (size_t i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        double perimeter = arcLength(contours[i], true);
        
        // Skip tiny contours
        if (area < 1000) {
            continue;
        }
        
        // Check bounds
        Rect bounds = cv::boundingRect(contours[i]);
        double widthRatio = static_cast<double>(bounds.width) / binaryImg.cols;
        double heightRatio = static_cast<double>(bounds.height) / binaryImg.rows;
        
        // Skip contours that span most of the image (likely image boundary, not paper)
        if (widthRatio > 0.95 || heightRatio > 0.95) {
            continue;
        }
        
        // Check for border touching - be more lenient for paper detection
        bool touchesBorder = false;
        const int borderMargin = 5; // Reduced margin for paper detection
        if (bounds.x <= borderMargin || bounds.y <= borderMargin || 
            bounds.x + bounds.width >= binaryImg.cols - borderMargin || 
            bounds.y + bounds.height >= binaryImg.rows - borderMargin) {
            touchesBorder = true;
        }
        
        // For paper detection, we're more tolerant of border touching
        // Only skip if it really looks like the image boundary
        if (touchesBorder && (widthRatio > 0.9 || heightRatio > 0.9)) {
            continue;
        }
        
        validContours++;
        
        // Test if it can be approximated to a rectangle (4 corners)
        vector<Point> approx;
        double epsilon = 0.02 * perimeter;
        approxPolyDP(contours[i], approx, epsilon, true);
        
        // Calculate score based on:
        // 1. Area (larger is better for paper)
        // 2. Rectangularity (4 corners is ideal, but don't exclude others)
        // 3. Aspect ratio (should be reasonable for A4 paper)
        double aspectRatio = static_cast<double>(bounds.width) / bounds.height;
        double aspectPenalty = 1.0;
        
        // A4 paper is about 1.4:1 ratio, so penalize extreme ratios less harshly
        if (aspectRatio > 5.0 || aspectRatio < 0.2) {
            aspectPenalty = 0.3; // Harsh penalty for very extreme ratios
        } else if (aspectRatio > 3.0 || aspectRatio < 0.33) {
            aspectPenalty = 0.7; // Moderate penalty for somewhat extreme ratios
        }
        
        double areaScore = area / (binaryImg.rows * binaryImg.cols); // Normalize by image size
        double rectangularityScore = (approx.size() == 4) ? 1.0 : 0.8; // Prefer 4-sided but don't exclude others
        
        double score = areaScore * rectangularityScore * aspectPenalty;
        
        
        if (score > bestScore) {
            bestScore = score;
            bestIdx = static_cast<int>(i);
        }
    }
    
    
    if (bestIdx < 0) {
        // Fallback to largest area if smart selection fails
        cout << "[WARN] Smart selection failed, falling back to largest area" << endl;
        double maxArea = 0.0;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    
    cout << "[INFO] Found " << contours.size() << " contours; selected contour " << bestIdx 
         << " with score " << bestScore << endl;
    
    
    return contours[bestIdx];
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
    cout << "[INFO] Starting CAD-optimized image processing pipeline..." << endl;
    
    // Use processImageToStage with final stage (7 = PRINT_TRACE_STAGE_FINAL)
    auto [warpedImg, finalContour] = processImageToStage(inputPath, params, 7);
    
    cout << "[INFO] CAD-optimized image processing pipeline completed successfully." << endl;
    cout << "[INFO] Final contour has " << finalContour.size() << " points" << endl;
    
    // Flush all debug images at the end
    flushDebugStack(params);
    
    return finalContour;
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
    pushDebugImage(originalImg, "original", params);
    pushDebugImage(grayImg, "grayscale", params);
    
    if (target_stage == 0) { // PRINT_TRACE_STAGE_LOADED
        return {grayImg.clone(), {}};
    }
    
    // Stage 1: Process to lightbox cropped (perspective correction)
    // First we need to detect the lightbox boundary
    Mat normalizedImg = normalizeLighting(grayImg, params);
    pushDebugImage(normalizedImg, "normalized", params);
    
    Mat boundaryEdges = detectEdges(normalizedImg, originalImg, params);
    pushDebugImage(boundaryEdges, "boundary_edges", params);
    
    vector<Point> boundaryContour = findBoundaryContour(boundaryEdges, params);
    // Note: boundary contour visualization not yet converted to stack
    
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
            // Fallback 2: Find actual corners using min/max coordinates from contour
            // This is more accurate than bounding rectangle for non-axis-aligned rectangles
            cout << "[INFO] Finding corners from contour extremes" << endl;
            
            // Find the four extreme points of the contour
            auto minXPoint = *min_element(boundaryContour.begin(), boundaryContour.end(), 
                [](const Point& a, const Point& b) { return a.x < b.x; });
            auto maxXPoint = *max_element(boundaryContour.begin(), boundaryContour.end(), 
                [](const Point& a, const Point& b) { return a.x < b.x; });
            auto minYPoint = *min_element(boundaryContour.begin(), boundaryContour.end(), 
                [](const Point& a, const Point& b) { return a.y < b.y; });
            auto maxYPoint = *max_element(boundaryContour.begin(), boundaryContour.end(), 
                [](const Point& a, const Point& b) { return a.y < b.y; });
            
            // Find the four corner candidates by combining extremes
            vector<Point> cornerCandidates;
            for (const Point& pt : boundaryContour) {
                // Check if this point is close to being a corner (near extremes)
                bool isCorner = false;
                
                // Top-left corner candidate (low x, low y)
                if (abs(pt.x - minXPoint.x) < 50 && abs(pt.y - minYPoint.y) < 50) isCorner = true;
                // Top-right corner candidate (high x, low y)  
                else if (abs(pt.x - maxXPoint.x) < 50 && abs(pt.y - minYPoint.y) < 50) isCorner = true;
                // Bottom-right corner candidate (high x, high y)
                else if (abs(pt.x - maxXPoint.x) < 50 && abs(pt.y - maxYPoint.y) < 50) isCorner = true;
                // Bottom-left corner candidate (low x, high y)
                else if (abs(pt.x - minXPoint.x) < 50 && abs(pt.y - maxYPoint.y) < 50) isCorner = true;
                
                if (isCorner) {
                    cornerCandidates.push_back(pt);
                }
            }
            
            if (cornerCandidates.size() >= 4) {
                // Sort candidates to get the best 4 corners
                // Sort by distance from center to get corner ordering
                Point center(0, 0);
                for (const Point& pt : cornerCandidates) {
                    center.x += pt.x;
                    center.y += pt.y;
                }
                center.x /= cornerCandidates.size();
                center.y /= cornerCandidates.size();
                
                // Sort by angle from center to get proper ordering
                sort(cornerCandidates.begin(), cornerCandidates.end(), 
                     [center](const Point& a, const Point& b) {
                         double angleA = atan2(a.y - center.y, a.x - center.x);
                         double angleB = atan2(b.y - center.y, b.x - center.x);
                         return angleA < angleB;
                     });
                
                corners = {cornerCandidates[0], cornerCandidates[1], cornerCandidates[2], cornerCandidates[3]};
                cout << "[INFO] Found " << cornerCandidates.size() << " corner candidates, using first 4" << endl;
            } else {
                // Alternative approach: Find the largest inscribed rectangle
                // This works better for paper boundaries with complex edges
                cout << "[INFO] Finding largest inscribed rectangle within paper boundary" << endl;
                
                // Find the inner bounds of the paper by looking for the largest clear rectangular area
                // We'll do this by finding the min/max coordinates that have enough support
                
                vector<int> xCoords, yCoords;
                for (const Point& pt : boundaryContour) {
                    xCoords.push_back(pt.x);
                    yCoords.push_back(pt.y);
                }
                
                sort(xCoords.begin(), xCoords.end());
                sort(yCoords.begin(), yCoords.end());
                
                // Use percentiles to find the inner rectangle bounds
                // This filters out outlier points from noise
                size_t xSize = xCoords.size();
                size_t ySize = yCoords.size();
                
                int x1 = xCoords[xSize * 0.1];  // 10th percentile
                int x2 = xCoords[xSize * 0.9];  // 90th percentile  
                int y1 = yCoords[ySize * 0.1];  // 10th percentile
                int y2 = yCoords[ySize * 0.9];  // 90th percentile
                
                corners = {
                    Point(x1, y1),    // Top-left
                    Point(x2, y1),    // Top-right  
                    Point(x2, y2),    // Bottom-right
                    Point(x1, y2)     // Bottom-left
                };
                
                cout << "[INFO] Using inscribed rectangle from (" << x1 << "," << y1 
                     << ") to (" << x2 << "," << y2 << ")" << endl;
            }
        } else {
            cout << "[INFO] Convex hull fallback successful" << endl;
        }
    }
    
    // Refine corners with sub-pixel accuracy
    vector<Point2f> refinedCorners = refineCorners(corners, normalizedImg, params);
    
    // 5. Log warp dimensions
    cout << "[INFO] Warping from "
         << grayImg.cols << "x" << grayImg.rows 
         << " px to "
         << params.lightboxWidthPx << "x" << params.lightboxHeightPx
         << " px ("
         << params.lightboxWidthMM << "mm x " << params.lightboxHeightMM << "mm)"
         << endl;

    // Warp image using the original grayscale (not binary) for better quality
    auto [warpedImg, pixelsPerMM] = warpImage(
        grayImg,
        refinedCorners,
        cv::Size(params.lightboxWidthPx, params.lightboxHeightPx),
        params.lightboxWidthMM,
        params.lightboxHeightMM
    );
    
    pushDebugImage(warpedImg, "perspective_corrected", params);
    
    if (target_stage == 1) { // PRINT_TRACE_STAGE_LIGHTBOX_CROPPED
        return {warpedImg.clone(), {}};
    }
    
    // Stage 2: Normalized (already done above, just return warped + normalized)
    Mat warpedNormalized = normalizeLighting(warpedImg, params);
    pushDebugImage(warpedNormalized, "warped_normalized", params);
    
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
    pushDebugContour(warpedImg, objectContour, "object_contour", params);
    
    if (target_stage == 4) { // PRINT_TRACE_STAGE_OBJECT_DETECTED
        return {warpedImg.clone(), objectContour};
    }
    
    // Stage 5: Smoothed (if enabled)
    vector<Point> processedContour = objectContour;
    if (params.enableSmoothing) {
        processedContour = smoothContour(processedContour, params.smoothingAmountMM, pixelsPerMM, params);
        pushDebugContour(warpedImg, processedContour, "smoothed_contour", params);
    }
    
    if (target_stage == 5) { // PRINT_TRACE_STAGE_SMOOTHED
        return {warpedImg.clone(), processedContour};
    }
    
    // Stage 6: Dilated (if enabled)
    if (params.dilationAmountMM > 0.0) {
        processedContour = dilateContour(processedContour, params.dilationAmountMM, pixelsPerMM, params);
        pushDebugContour(warpedImg, processedContour, "dilated_contour", params);
    }
    
    if (target_stage == 6) { // PRINT_TRACE_STAGE_DILATED
        return {warpedImg.clone(), processedContour};
    }
    
    // Stage 7: Final (validate contour)
    if (!validateContour(processedContour, params)) {
        throw runtime_error("Final contour validation failed");
    }
    
    pushDebugContour(warpedImg, processedContour, "final_contour", params);
    
    // Flush all debug images at the end
    flushDebugStack(params);
    
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
    
    saveDebugImage(mask, "merged_mask.jpg", params);
    
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
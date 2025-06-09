#include "DXFWriter.hpp"
#include <iostream>
#include <memory>

namespace OutlineTool {

DXFWriter::DXFWriter(dxfRW& dxfWriter, double pixelsPerMM) 
    : m_dxfWriter(dxfWriter), m_pixelsPerMM(pixelsPerMM) {
}

void DXFWriter::addContour(const std::vector<cv::Point>& contour) {
    DRW_LWPolyline polyline;
    polyline.layer = "Default";
    polyline.color = 256; // By layer
    polyline.flags = 1;   // Closed polyline
    polyline.elevation = 0.0;
    polyline.thickness = 0.0;

    for (const auto& point : contour) {
        DRW_Vertex2D vertex;
        vertex.x = static_cast<double>(point.x) / m_pixelsPerMM;
        vertex.y = static_cast<double>(point.y) / m_pixelsPerMM;
        vertex.bulge = 0.0;
        polyline.addVertex(vertex);
    }

    m_polylines.push_back(polyline);
}

void DXFWriter::addLWPolyline(const DRW_LWPolyline& data) {
    m_polylines.push_back(data);
}

void DXFWriter::writeEntities() {
    for (auto& polyline : m_polylines) {
        if (!m_dxfWriter.writeLWPolyline(&polyline)) {
            std::cerr << "[ERROR] Failed to write LWPolyline to DXF." << std::endl;
        }
    }
    m_polylines.clear();
}

bool DXFWriter::saveContourAsDXF(const std::vector<cv::Point>& contour, 
                                 double pixelsPerMM, 
                                 const std::string& outputPath) {
    std::cout << "[INFO] Saving contour to DXF: " << outputPath << std::endl;

    try {
        dxfRW dxf(outputPath.c_str());
        DXFWriter writer(dxf, pixelsPerMM);
        writer.addContour(contour);

        if (!dxf.write(&writer, DRW::Version::AC1015, false)) {
            std::cerr << "[ERROR] Failed to write DXF file." << std::endl;
            return false;
        }

        std::cout << "[INFO] DXF file saved successfully." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception while saving DXF: " << e.what() << std::endl;
        return false;
    }
}

} // namespace OutlineTool
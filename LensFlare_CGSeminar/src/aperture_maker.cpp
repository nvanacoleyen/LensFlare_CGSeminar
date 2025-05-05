#include "aperture_maker.h" 
#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <imgui/imgui.h>


int numBlades = 12;
int angleDeg = 0;
const int imgSize = 512; //texture resolution
cv::Mat image;
GLuint aptTexture = 0;

void updateAptImage() {
    //min 3 blades
    int effectiveBlades = (numBlades < 3 ? 3 : numBlades);

    image = cv::Mat(imgSize, imgSize, CV_8UC1, cv::Scalar(0));
    cv::Point center(imgSize / 2, imgSize / 2);

    //Use a base radius set to 30% of the image’s size.
    double baseRadius = imgSize * 0.3;
    double angleOffset = angleDeg * CV_PI / 180.0;

    //Compute the minimal scale factor so that at least one vertex reaches an edge.
    double s_min = std::numeric_limits<double>::infinity();
    for (int i = 0; i < effectiveBlades; i++) {
        double theta = angleOffset + 2.0 * CV_PI * i / effectiveBlades;
        double cosTheta = std::cos(theta);
        double sinTheta = std::sin(theta);
        double candidate_x = (std::abs(cosTheta) < 1e-6)
            ? std::numeric_limits<double>::infinity()
            : (static_cast<double>(center.x) / (baseRadius * std::abs(cosTheta)));
        double candidate_y = (std::abs(sinTheta) < 1e-6)
            ? std::numeric_limits<double>::infinity()
            : (static_cast<double>(center.y) / (baseRadius * std::abs(sinTheta)));
        double candidate = std::min(candidate_x, candidate_y);
        s_min = std::min(s_min, candidate);
    }
    double newRadius = baseRadius * s_min;

    //Compute the polygon vertices.
    std::vector<cv::Point> vertices;
    for (int i = 0; i < effectiveBlades; i++) {
        double theta = angleOffset + 2.0 * CV_PI * i / effectiveBlades;
        int x = static_cast<int>(center.x + newRadius * std::cos(theta));
        int y = static_cast<int>(center.y + newRadius * std::sin(theta));
        vertices.push_back(cv::Point(x, y));
    }

    std::vector<std::vector<cv::Point>> pts{ vertices };
    cv::fillPoly(image, pts, cv::Scalar(255));
}

void updateAptTexture(GLuint texApt) {
    glTextureSubImage2D(texApt, 0, 0, 0, image.cols, image.rows, GL_RED, GL_UNSIGNED_BYTE, image.ptr());
}

void saveTex() {
    std::string filename = "resources/iris.png";
    if (cv::imwrite(filename, image)) {
        std::cout << "Image saved as " << filename << std::endl;
    }
    else {
        std::cerr << "Error: Could not save image." << std::endl;
    }
}

void createApt(bool* p_open, GLuint texApt) {
    if (ImGui::BeginPopupModal("Aperture Generator", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Configure the aperture parameters:");

        if (ImGui::SliderInt("Blades (min 3)", &numBlades, 3, 30)) {
            updateAptImage();
            updateAptTexture(texApt);
        }

        if (ImGui::SliderInt("Angle (deg)", &angleDeg, 0, 180)) {
            updateAptImage();
            updateAptTexture(texApt);
        }

        if (ImGui::Button("Load")) {
            *p_open = false;
            saveTex();
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();

        if (texApt != 0) {
            ImGui::Image((void*)(intptr_t)texApt, ImVec2((float)image.cols, (float)image.rows));
        }
        ImGui::EndPopup();
    }
}

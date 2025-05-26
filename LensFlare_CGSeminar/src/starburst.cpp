#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>
#include "utils.h" 
#include <random>

// Random number generator for angle variation
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> angleDist(-CV_PI / 30.0, CV_PI / 30.0);


int createStarburst(const char* apertureLocation) {
    cv::Mat aperture = cv::imread(apertureLocation, cv::IMREAD_GRAYSCALE);
    if (aperture.empty()) {
        std::cerr << "Failed to load aperture texture." << std::endl;
        return -1;
    }

    // Expand to optimal size for DFT
    int m = cv::getOptimalDFTSize(aperture.rows);
    int n = cv::getOptimalDFTSize(aperture.cols);
    cv::copyMakeBorder(aperture, aperture, 0, m - aperture.rows, 0, n - aperture.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

    // Perform DFT
    cv::Mat planes[] = { cv::Mat_<float>(aperture), cv::Mat::zeros(aperture.size(), CV_32F) };
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);
    cv::dft(complexI, complexI);

    // Compute the power spectrum
    cv::split(complexI, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat magnitudeImage = planes[0];
    cv::Mat powerSpectrum;
    cv::pow(magnitudeImage, 2, powerSpectrum);


    // Rearrange the quadrants of Fourier image
    int cx = powerSpectrum.cols / 2;
    int cy = powerSpectrum.rows / 2;
    cv::Mat q0(powerSpectrum, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(powerSpectrum, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(powerSpectrum, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(powerSpectrum, cv::Rect(cx, cy, cx, cy));

    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);

    cv::normalize(powerSpectrum, powerSpectrum, 0, 1, cv::NORM_MINMAX);

    cv::Mat starburstTexture = cv::Mat::zeros(powerSpectrum.size(), CV_32F);
    float intensity = 1000000.f;

    for (double lambda = 380; lambda <= 750.0; lambda += 5.0) {
        double scale = lambda / 750.0;
        double invScale = 1.0 / scale;

        int centerX = starburstTexture.cols / 2;
        int centerY = starburstTexture.rows / 2;

        double angle = angleDist(gen); // Random angle per wavelength
        double cosA = std::cos(angle);
        double sinA = std::sin(angle);

        for (int y = 0; y < starburstTexture.rows; ++y) {
            for (int x = 0; x < starburstTexture.cols; ++x) {
                int dx = x - centerX;
                int dy = y - centerY;

                // Apply rotation
                double rdx = dx * cosA - dy * sinA;
                double rdy = dx * sinA + dy * cosA;

                // Apply inverse scaling
                int srcX = static_cast<int>(rdx * invScale + powerSpectrum.cols / 2);
                int srcY = static_cast<int>(rdy * invScale + powerSpectrum.rows / 2);

                if (srcX >= 0 && srcX < powerSpectrum.cols && srcY >= 0 && srcY < powerSpectrum.rows) {
                    float value = powerSpectrum.at<float>(srcY, srcX);
                    starburstTexture.at<float>(y, x) += intensity * value;
                }
            }
        }

    }


    // Gaussian filter to smooth the texture
    //cv::Mat starburstTextureFiltered;
    //cv::GaussianBlur(starburstTexture, starburstTextureFiltered, cv::Size(3, 3), 0);

    if (!cv::imwrite("resources/starburst_texture.png", starburstTexture)) {
        std::cerr << "Failed to save starburst texture." << std::endl;
        return -1;
    }
    return 0;
}

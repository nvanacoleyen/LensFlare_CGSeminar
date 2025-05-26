#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>
#include "utils.h" 

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

    // Compute the magnitude (power spectrum)
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

    cv::Mat starburstTexture = cv::Mat::zeros(powerSpectrum.size(), CV_32FC3);

    //double lambda0 = 565.0;
    // Iterate over wavelengths from 380 to 750
    for (double lambda = 380; lambda <= 750.0; lambda += 5.0) {
        double scale = 200 * lambda;  // Scale factor to boost intensity
        cv::Mat scaledMagnitude;
        cv::multiply(powerSpectrum, scale, scaledMagnitude);
        glm::vec3 color = wavelengthToRGB(lambda);

        // For each pixel in the Fourier image, we remap its coordinate:
        for (int y = 0; y < powerSpectrum.rows; ++y) {
            for (int x = 0; x < powerSpectrum.cols; ++x) {
                float intensity = scaledMagnitude.at<float>(y, x);
                starburstTexture.at<cv::Vec3f>(y, x) += intensity * cv::Vec3f(color.r, color.g, color.b);
            }
        }
    }

    // Gaussian filter to smooth the texture
    cv::Mat starburstTextureFiltered;
    cv::GaussianBlur(starburstTexture, starburstTextureFiltered, cv::Size(3, 3), 0);

    // Convert from BGR to RGB (if needed)
    cv::Mat starburstTextureRGB;
    cv::cvtColor(starburstTextureFiltered, starburstTextureRGB, cv::COLOR_BGR2RGB);

    if (!cv::imwrite("resources/starburst_texture.png", starburstTextureRGB)) {
        std::cerr << "Failed to save starburst texture." << std::endl;
        return -1;
    }
    return 0;
}

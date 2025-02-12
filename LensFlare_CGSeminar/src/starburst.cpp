#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>
#include "utils.h"

int createStarburst(const char* apertureLocation) {
    // Load the aperture texture
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

    // Rearrange the quadrants of Fourier image
    int cx = magnitudeImage.cols / 2;
    int cy = magnitudeImage.rows / 2;

    cv::Mat q0(magnitudeImage, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(magnitudeImage, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(magnitudeImage, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(magnitudeImage, cv::Rect(cx, cy, cx, cy));

    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);

    cv::normalize(magnitudeImage, magnitudeImage, 0, 1, cv::NORM_MINMAX);

    // Prepare an RGB texture by superimposing scaled spectra for different wavelengths
    cv::Mat starburstTexture = cv::Mat::zeros(magnitudeImage.size(), CV_32FC3);

    // Wavelengths from 380 nm to 750 nm (visible spectrum)
    for (double lambda = 380.0; lambda <= 750.0; lambda += 5.0) {
        double scale = 10 /* Define your scaling based on wavelength */;
        cv::Mat scaledMagnitude;
        cv::multiply(magnitudeImage, scale, scaledMagnitude);

        // Convert wavelength to RGB
        glm::vec3 color = wavelengthToRGB(lambda);

        // Accumulate into the starburst texture
        for (int y = 0; y < starburstTexture.rows; ++y) {
            for (int x = 0; x < starburstTexture.cols; ++x) {
                float intensity = scaledMagnitude.at<float>(y, x);
                starburstTexture.at<cv::Vec3f>(y, x) += intensity * cv::Vec3f(color.x, color.y, color.z);
            }
        }
    }

    // Normalize the final texture
    //cv::normalize(starburstTexture, starburstTexture, 0, 1, cv::NORM_MINMAX);

    // Save the starburst texture
    cv::imwrite("resources/starburst_texture.png", starburstTexture);

    return 0;
}
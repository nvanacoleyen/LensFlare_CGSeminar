#include "preset_lens_systems.h"

LensSystem heliarTronerLens() {

    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, coating
    lensInterfaces.push_back(LensInterface(7.7f, 1.652f, 30.81f, 430)); //LAKN7
    lensInterfaces.push_back(LensInterface(1.85f, 1.603f, -89.35f, 370)); //F5
    lensInterfaces.push_back(LensInterface(3.52f, 1.f, 580.38f, 510)); //air
    lensInterfaces.push_back(LensInterface(1.85f, 1.643f, -80.63f, 600)); //BAF9
    lensInterfaces.push_back(LensInterface(4.18f, 1.f, 28.34f, 310)); //air
    lensInterfaces.push_back(LensInterface(3.0f, 1.f, std::numeric_limits<float>::infinity(), 460)); //air (iris aperture)
    lensInterfaces.push_back(LensInterface(1.85f, 1.581f, std::numeric_limits<float>::infinity(), 550)); //LF5
    lensInterfaces.push_back(LensInterface(7.27f, 1.694f, 32.19f, 580)); //LAK13
    lensInterfaces.push_back(LensInterface(81.857f, 1.f, -52.99f, 650)); //air

    return LensSystem(5, 11.6f, 14.5f, lensInterfaces);

}

/* Canon 28-80 f/2.8 (US5576890) */
LensSystem someCanonLens() {

    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, coating
    lensInterfaces.push_back(LensInterface(2.62f, 1.80518f, 684.66f, 403)); 
    lensInterfaces.push_back(LensInterface(0.2f, 1.f, -1055.76f, 301));
    lensInterfaces.push_back(LensInterface(2.1f, 1.713f, 149.76f, 590));
    lensInterfaces.push_back(LensInterface(18.02f, 1.f, 53.30f, 550));

    lensInterfaces.push_back(LensInterface(2.0f, 1.77250f, -488.25f, 380));
    lensInterfaces.push_back(LensInterface(0.53f, 1.f, 44.81, 420));
    lensInterfaces.push_back(LensInterface(3.5f, 1.84666f, 43.27, 490));
    lensInterfaces.push_back(LensInterface(40.13f, 1.f, 78.34f, 300));

    lensInterfaces.push_back(LensInterface(1.2f, 1.84666f, 84.43f, 440));
    lensInterfaces.push_back(LensInterface(7.2f, 1.55963f, 30.98f, 650));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -1529.08f, 710));
    lensInterfaces.push_back(LensInterface(6.f, 1.65160f, 50.67f, 420));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -110.42f, 510));
    lensInterfaces.push_back(LensInterface(3.3f, 1.65160f, 40.57f, 540));
    lensInterfaces.push_back(LensInterface(6.91f, 1.f, 71.98f, 230));

    lensInterfaces.push_back(LensInterface(1.50f, 1.f, std::numeric_limits<float>::infinity()));
    lensInterfaces.push_back(LensInterface(3.f, 1.84666f, -145.1f, 570));
    lensInterfaces.push_back(LensInterface(1.2f, 1.60311f, -34.13f, 630));
    lensInterfaces.push_back(LensInterface(2.f, 1.f, 112.83f, 490));
    lensInterfaces.push_back(LensInterface(1.4f, 1.60311f, -42.83f, 450));
    lensInterfaces.push_back(LensInterface(13.17f, 1.f, 66.44f, 530));

    lensInterfaces.push_back(LensInterface(5.f, 1.55963f, 347.07f, 390));
    lensInterfaces.push_back(LensInterface(1.5f, 1.80518f, -26.27f, 420));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -35.22f, 220));
    lensInterfaces.push_back(LensInterface(5.f, 1.713f, 104.39f, 680));
    lensInterfaces.push_back(LensInterface(5.44f, 1.f, -51.25f, 700));
    lensInterfaces.push_back(LensInterface(1.35f, 1.84666f, -30.94f, 560));
    lensInterfaces.push_back(LensInterface(80.f, 1.f, -84.63f, 430));

    return LensSystem(15, 10.5f, 14.5f, lensInterfaces);

}

LensSystem testLens() {
    float coatingwavelength = 440;
    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, coating
    lensInterfaces.push_back(LensInterface(7.7f, 1.652f, 30.81f, coatingwavelength)); //LAKN7
    lensInterfaces.push_back(LensInterface(1.85f, 1.603f, -89.35f, coatingwavelength)); //F5
    lensInterfaces.push_back(LensInterface(3.52f, 1.f, 580.38f, coatingwavelength)); //air
    lensInterfaces.push_back(LensInterface(1.85f, 1.643f, -80.63f, coatingwavelength)); //BAF9
    lensInterfaces.push_back(LensInterface(4.18f, 1.f, 28.34f, coatingwavelength)); //air
    lensInterfaces.push_back(LensInterface(3.0f, 1.f, std::numeric_limits<float>::infinity(), coatingwavelength)); //air (iris aperture)
    lensInterfaces.push_back(LensInterface(1.85f, 1.581f, std::numeric_limits<float>::infinity(), coatingwavelength)); //LF5
    lensInterfaces.push_back(LensInterface(7.27f, 1.694f, 32.19f, coatingwavelength)); //LAK13
    lensInterfaces.push_back(LensInterface(81.857f, 1.f, -52.99f, coatingwavelength)); //air

    return LensSystem(5, 11.6f, 14.5f, lensInterfaces);

}
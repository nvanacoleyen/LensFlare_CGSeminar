#include "preset_lens_systems.h"

LensSystem heliarTronerLens() {

    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, height
    lensInterfaces.push_back(LensInterface(7.7f, 1.652f, 30.81f, 14.5f)); //LAKN7
    lensInterfaces.push_back(LensInterface(1.85f, 1.603f, -89.35f, 14.5f)); //F5
    lensInterfaces.push_back(LensInterface(3.52f, 1.f, 580.38f, 14.5f)); //air
    lensInterfaces.push_back(LensInterface(1.85f, 1.643f, -80.63f, 12.3f)); //BAF9
    lensInterfaces.push_back(LensInterface(4.18f, 1.f, 28.34f, 12.f)); //air
    lensInterfaces.push_back(LensInterface(3.0f, 1.f, std::numeric_limits<float>::infinity(), 11.6f)); //air (iris aperture)
    lensInterfaces.push_back(LensInterface(1.85f, 1.581f, std::numeric_limits<float>::infinity(), 12.3f)); //LF5
    lensInterfaces.push_back(LensInterface(7.27f, 1.694f, 32.19f, 12.3f)); //LAK13
    lensInterfaces.push_back(LensInterface(81.857f, 1.f, -52.99f, 12.3f)); //air

    return LensSystem(5, 10.f, lensInterfaces);

}

/* Canon 28-80 f/2.8 (US5576890) */
LensSystem someCanonLens() {

    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, height
    lensInterfaces.push_back(LensInterface(2.62f, 1.80518f, 684.66f, 14.5f)); 
    lensInterfaces.push_back(LensInterface(0.2f, 1.f, -1055.76f, 14.5f));
    lensInterfaces.push_back(LensInterface(2.1f, 1.713f, 149.76f, 14.5f));
    lensInterfaces.push_back(LensInterface(18.02f, 1.f, 53.30f, 14.5f));

    lensInterfaces.push_back(LensInterface(2.0f, 1.77250f, -488.25f, 14.5f));
    lensInterfaces.push_back(LensInterface(0.53f, 1.f, 44.81, 14.5f));
    lensInterfaces.push_back(LensInterface(3.5f, 1.84666f, 43.27, 14.5f));
    lensInterfaces.push_back(LensInterface(40.13f, 1.f, 78.34f, 14.5f));

    lensInterfaces.push_back(LensInterface(1.2f, 1.84666f, 84.43f, 14.5f));
    lensInterfaces.push_back(LensInterface(7.2f, 1.55963f, 30.98f, 14.5f));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -1529.08f, 14.5f));
    lensInterfaces.push_back(LensInterface(6.f, 1.65160f, 50.67f, 14.5f));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -110.42f, 14.5f));
    lensInterfaces.push_back(LensInterface(3.3f, 1.65160f, 40.57f, 14.5f));
    lensInterfaces.push_back(LensInterface(6.91f, 1.f, 71.98f, 14.5f));

    lensInterfaces.push_back(LensInterface(1.50f, 1.f, std::numeric_limits<float>::infinity(), 10.5f));
    lensInterfaces.push_back(LensInterface(3.f, 1.84666f, -145.1f, 14.5f));
    lensInterfaces.push_back(LensInterface(1.2f, 1.60311f, -34.13f, 14.5f));
    lensInterfaces.push_back(LensInterface(2.f, 1.f, 112.83f, 14.5f));
    lensInterfaces.push_back(LensInterface(1.4f, 1.60311f, -42.83f, 14.5f));
    lensInterfaces.push_back(LensInterface(13.17f, 1.f, 66.44f, 14.5f));

    lensInterfaces.push_back(LensInterface(5.f, 1.55963f, 347.07f, 14.5f));
    lensInterfaces.push_back(LensInterface(1.5f, 1.80518f, -26.27f, 14.5f));
    lensInterfaces.push_back(LensInterface(0.15f, 1.f, -35.22f, 14.5f));
    lensInterfaces.push_back(LensInterface(5.f, 1.713f, 104.39f, 14.5f));
    lensInterfaces.push_back(LensInterface(5.44f, 1.f, -51.25f, 14.5f));
    lensInterfaces.push_back(LensInterface(1.35f, 1.84666f, -30.94f, 14.5f));
    lensInterfaces.push_back(LensInterface(80.f, 1.f, -84.63f, 14.5f));

    return LensSystem(15, 10.f, lensInterfaces);

}
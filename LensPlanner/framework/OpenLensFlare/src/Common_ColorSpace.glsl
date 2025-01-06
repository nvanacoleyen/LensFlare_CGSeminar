// xyz <-> RGB conversion
mat3 cieRGB2xyz = mat3
( 
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);

mat3 ciexyz2RGB = mat3
(
     3.2404542, -0.9692660, 0.0556434,
     -1.5371385, 1.8760108, -0.2040259,
     -0.4985314, 0.0415560, 1.0572252 
);

vec3 RGB2xyz(vec3 rgb)
{
    return cieRGB2xyz * rgb;
}

vec3 xyz2RGB(vec3 xyz)
{
    return clamp(ciexyz2RGB * xyz, vec3(0.0), vec3(1.0));
}

// CIE color matching function evaluated at 5 nm steps from 380 to 780 nm
vec3 cieColorMatch5[81];

void initializeCieColorMatch5() {
    cieColorMatch5[0] = vec3(0.0014, 0.0000, 0.0065);
    cieColorMatch5[1] = vec3(0.0022, 0.0001, 0.0105);
    cieColorMatch5[2] = vec3(0.0042, 0.0001, 0.0201);
    cieColorMatch5[3] = vec3(0.0076, 0.0002, 0.0362);
    cieColorMatch5[4] = vec3(0.0143, 0.0004, 0.0679);
    cieColorMatch5[5] = vec3(0.0232, 0.0006, 0.1102);
    cieColorMatch5[6] = vec3(0.0435, 0.0012, 0.2074);
    cieColorMatch5[7] = vec3(0.0776, 0.0022, 0.3713);
    cieColorMatch5[8] = vec3(0.1344, 0.0040, 0.6456);
    cieColorMatch5[9] = vec3(0.2148, 0.0073, 1.0391);
    cieColorMatch5[10] = vec3(0.2839, 0.0116, 1.3856);
    cieColorMatch5[11] = vec3(0.3285, 0.0168, 1.6230);
    cieColorMatch5[12] = vec3(0.3483, 0.0230, 1.7471);
    cieColorMatch5[13] = vec3(0.3481, 0.0298, 1.7826);
    cieColorMatch5[14] = vec3(0.3362, 0.0380, 1.7721);
    cieColorMatch5[15] = vec3(0.3187, 0.0480, 1.7441);
    cieColorMatch5[16] = vec3(0.2908, 0.0600, 1.6692);
    cieColorMatch5[17] = vec3(0.2511, 0.0739, 1.5281);
    cieColorMatch5[18] = vec3(0.1954, 0.0910, 1.2876);
    cieColorMatch5[19] = vec3(0.1421, 0.1126, 1.0419);
    cieColorMatch5[20] = vec3(0.0956, 0.1390, 0.8130);
    cieColorMatch5[21] = vec3(0.0580, 0.1693, 0.6162);
    cieColorMatch5[22] = vec3(0.0320, 0.2080, 0.4652);
    cieColorMatch5[23] = vec3(0.0147, 0.2586, 0.3533);
    cieColorMatch5[24] = vec3(0.0049, 0.3230, 0.2720);
    cieColorMatch5[25] = vec3(0.0024, 0.4073, 0.2123);
    cieColorMatch5[26] = vec3(0.0093, 0.5030, 0.1582);
    cieColorMatch5[27] = vec3(0.0291, 0.6082, 0.1117);
    cieColorMatch5[28] = vec3(0.0633, 0.7100, 0.0782);
    cieColorMatch5[29] = vec3(0.1096, 0.7932, 0.0573);
    cieColorMatch5[30] = vec3(0.1655, 0.8620, 0.0422);
    cieColorMatch5[31] = vec3(0.2257, 0.9149, 0.0298);
    cieColorMatch5[32] = vec3(0.2904, 0.9540, 0.0203);
    cieColorMatch5[33] = vec3(0.3597, 0.9803, 0.0134);
    cieColorMatch5[34] = vec3(0.4334, 0.9950, 0.0087);
    cieColorMatch5[35] = vec3(0.5121, 1.0000, 0.0057);
    cieColorMatch5[36] = vec3(0.5945, 0.9950, 0.0039);
    cieColorMatch5[37] = vec3(0.6784, 0.9786, 0.0027);
    cieColorMatch5[38] = vec3(0.7621, 0.9520, 0.0021);
    cieColorMatch5[39] = vec3(0.8425, 0.9154, 0.0018);
    cieColorMatch5[40] = vec3(0.9163, 0.8700, 0.0017);
    cieColorMatch5[41] = vec3(0.9786, 0.8163, 0.0014);
    cieColorMatch5[42] = vec3(1.0263, 0.7570, 0.0011);
    cieColorMatch5[43] = vec3(1.0567, 0.6949, 0.0010);
    cieColorMatch5[44] = vec3(1.0622, 0.6310, 0.0008);
    cieColorMatch5[45] = vec3(1.0456, 0.5668, 0.0006);
    cieColorMatch5[46] = vec3(1.0026, 0.5030, 0.0003);
    cieColorMatch5[47] = vec3(0.9384, 0.4412, 0.0002);
    cieColorMatch5[48] = vec3(0.8544, 0.3810, 0.0002);
    cieColorMatch5[49] = vec3(0.7514, 0.3210, 0.0001);
    cieColorMatch5[50] = vec3(0.6424, 0.2650, 0.0000);
    cieColorMatch5[51] = vec3(0.5419, 0.2170, 0.0000);
    cieColorMatch5[52] = vec3(0.4479, 0.1750, 0.0000);
    cieColorMatch5[53] = vec3(0.3608, 0.1382, 0.0000);
    cieColorMatch5[54] = vec3(0.2835, 0.1070, 0.0000);
    cieColorMatch5[55] = vec3(0.2187, 0.0816, 0.0000);
    cieColorMatch5[56] = vec3(0.1649, 0.0610, 0.0000);
    cieColorMatch5[57] = vec3(0.1212, 0.0446, 0.0000);
    cieColorMatch5[58] = vec3(0.0874, 0.0320, 0.0000);
    cieColorMatch5[59] = vec3(0.0636, 0.0232, 0.0000);
    cieColorMatch5[60] = vec3(0.0468, 0.0170, 0.0000);
    cieColorMatch5[61] = vec3(0.0329, 0.0119, 0.0000);
    cieColorMatch5[62] = vec3(0.0227, 0.0082, 0.0000);
    cieColorMatch5[63] = vec3(0.0158, 0.0057, 0.0000);
    cieColorMatch5[64] = vec3(0.0114, 0.0041, 0.0000);
    cieColorMatch5[65] = vec3(0.0081, 0.0029, 0.0000);
    cieColorMatch5[66] = vec3(0.0058, 0.0021, 0.0000);
    cieColorMatch5[67] = vec3(0.0041, 0.0015, 0.0000);
    cieColorMatch5[68] = vec3(0.0029, 0.0010, 0.0000);
    cieColorMatch5[69] = vec3(0.0020, 0.0007, 0.0000);
    cieColorMatch5[70] = vec3(0.0014, 0.0005, 0.0000);
    cieColorMatch5[71] = vec3(0.0010, 0.0004, 0.0000);
    cieColorMatch5[72] = vec3(0.0007, 0.0002, 0.0000);
    cieColorMatch5[73] = vec3(0.0005, 0.0002, 0.0000);
    cieColorMatch5[74] = vec3(0.0003, 0.0001, 0.0000);
    cieColorMatch5[75] = vec3(0.0002, 0.0001, 0.0000);
    cieColorMatch5[76] = vec3(0.0002, 0.0001, 0.0000);
    cieColorMatch5[77] = vec3(0.0001, 0.0000, 0.0000);
    cieColorMatch5[78] = vec3(0.0001, 0.0000, 0.0000);
    cieColorMatch5[79] = vec3(0.0001, 0.0000, 0.0000);
    cieColorMatch5[80] = vec3(0.0000, 0.0000, 0.0000);
}


// Wavelength -> XYZ and RGB conversion
vec3 lambda2XYZ(float lambda, float intensity)
{
    int id = clamp(int((lambda - 380.0) * 0.2), 0, 80);
    initializeCieColorMatch5();
    return intensity * cieColorMatch5[id];
}

vec3 XYZ2xyz(vec3 XYZ)
{
    return XYZ / (XYZ.x + XYZ.y + XYZ.z);
}

vec3 lambda2RGB(float lambda, float intensity)
{
    return xyz2RGB(XYZ2xyz(lambda2XYZ(lambda, intensity)));
}

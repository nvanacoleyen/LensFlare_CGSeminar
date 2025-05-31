#pragma OPENCL EXTENSION cl_khr_fp64 : enable  // Enable double precision

//---------------------------------------------------------------------
// Helper: Multiply two 2×2 matrices (in column‑major order).
// A and B are 4-element float arrays; result R = A * B is computed as:
//   R[0] = A[0]*B[0] + A[2]*B[1]
//   R[1] = A[1]*B[0] + A[3]*B[1]
//   R[2] = A[0]*B[2] + A[2]*B[3]
//   R[3] = A[1]*B[2] + A[3]*B[3]
inline void mat2_mul(const float A[4], const float B[4], float R[4]) {
    R[0] = A[0] * B[0] + A[2] * B[1];
    R[1] = A[1] * B[0] + A[3] * B[1];
    R[2] = A[0] * B[2] + A[2] * B[3];
    R[3] = A[1] * B[2] + A[3] * B[3];
}

//---------------------------------------------------------------------
// Returns the translation matrix for parameter di.
// In our representation, the translation matrix is:
//   [1.0      di]
//   [0.0     1.0 ]
// Stored as: {1.0f, 0.0f, di, 1.0f}
inline void getTranslationMatrix(float di, float M[4]) {
    M[0] = 1.0f;  // first column, row0
    M[1] = 0.0f;  // first column, row1
    M[2] = di;    // second column, row0
    M[3] = 1.0f;  // second column, row1
}

//---------------------------------------------------------------------
// Returns the refraction matrix for indices n1, n2 and radius Ri.
// If Ri equals zero, we substitute it with infinity (using INFINITY).
// The matrix is defined as:
//   [ 1.0         0.0 ]
//   [ thirdTerm  fourthTerm ]
// where thirdTerm = (n1 - n2) / (n2 * Ri)
//       fourthTerm = n1 / n2
// In our array (column-major): {1.0, thirdTerm, 0.0, fourthTerm}
inline void getRefractionMatrix(float n1, float n2, float Ri, float M[4]) {
    if (Ri == 0.0f) {
        Ri = INFINITY;  // or use a very large number if preferred
    }
    float thirdTerm = (n1 - n2) / (n2 * Ri);
    float fourthTerm = n1 / n2;
    M[0] = 1.0f;
    M[1] = thirdTerm;
    M[2] = 0.0f;
    M[3] = fourthTerm;
}

//---------------------------------------------------------------------
// Returns the reflection matrix for a given Ri.  If Ri == 0,
// we substitute with infinity.
// The matrix is defined as:
//   [1.0         0.0]
//   [2.0/Ri      1.0]
// Stored as: {1.0f, 2.0f / Ri, 0.0f, 1.0f}
inline void getReflectionMatrix(float Ri, float M[4]) {
    if (Ri == 0.0f) {
        Ri = INFINITY;
    }
    M[0] = 1.0f;
    M[1] = 2.0f / Ri;
    M[2] = 0.0f;
    M[3] = 1.0f;
}

//---------------------------------------------------------------------
// Returns the product of a translation matrix and a refraction matrix,
// i.e. getTranslationMatrix(di) * getRefractionMatrix(n1, n2, Ri)
// The result is stored in R.
inline void getTranslationRefractionMatrix(float di, float n1, float n2, float Ri, float R[4]) {
    float T[4], Rfr[4];
    getTranslationMatrix(di, T);
    getRefractionMatrix(n1, n2, Ri, Rfr);
    mat2_mul(T, Rfr, R);
}

//---------------------------------------------------------------------
// Returns the product of getRefractionMatrix(n2, n1, -Ri) and the 
// translation matrix for di, i.e.:
// getRefractionMatrix(n2, n1, -Ri) * getTranslationMatrix(di)
// The result is stored in R.
inline void getinverseRefractionBackwardsTranslationMatrix(float di, float n1, float n2, float Ri, float R[4]) {
    float Rfr[4], T[4];
    getRefractionMatrix(n2, n1, -Ri, Rfr);
    getTranslationMatrix(di, T);
    mat2_mul(Rfr, T, R);
}

// Helper: Multiply a 2x2 matrix (represented as a 4-element array)
// with a 2D vector. The matrix is assumed to be in column-major order:
//   [ M[0]  M[2] ]
//   [ M[1]  M[3] ]
inline float2 mat2_mul_vec2(const float M[4], float2 v) {
    float2 r;
    r.x = M[0] * v.x + M[2] * v.y;
    r.y = M[1] * v.x + M[3] * v.y;
    return r;
}

//=====================================================================
// Helper functions to compute system matrices from lens interfaces.
// We assume lens interfaces are stored in a flat array:
//   lens_interfaces[0] = di0, [1] = ni0, [2] = Ri0, then di1, ni1, Ri1, etc.
// The total number of interfaces is num_interfaces.
// The iris aperture position is given by iris_pos.
//----------------------------------------------------------------------
// computeMa: Propagation from the start up to the iris (no reflection).
//
// C++ equivalent:
//   glm::mat2x2 Ma = I;
//   for (int i=0; i < m_iris_aperture_pos; i++) {
//     if (i==0)
//       Ma = getTranslationRefractionMatrix(lens_interfaces[0].di, 1.f, lens_interfaces[0].ni, lens_interfaces[0].Ri) * Ma;
//     else
//       Ma = getTranslationRefractionMatrix(lens_interfaces[i].di, lens_interfaces[i-1].ni, lens_interfaces[i].ni, lens_interfaces[i].Ri) * Ma;
//   }
inline void computeMa(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    float Ma[4])
{
    // Initialize Ma as identity.
    Ma[0] = 1.0f; Ma[1] = 0.0f; Ma[2] = 0.0f; Ma[3] = 1.0f;
    for (int i = 0; i < iris_pos; i++) {
        float T[4];
        if (i == 0) {
            // For the first interface, the "previous" refractive index is 1.0.
            getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                1.0f,
                lens_interfaces[3 * i + 1],
                lens_interfaces[3 * i + 2],
                T);
        }
        else {
            getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                lens_interfaces[3 * (i - 1) + 1],
                lens_interfaces[3 * i + 1],
                lens_interfaces[3 * i + 2],
                T);
        }
        float temp[4];
        mat2_mul(T, Ma, temp);
        Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
    }
}

// Helper lambdas (implemented as inline functions) to select “effective” values.
// In your C++ code these were implemented with lambdas.
inline float effective_ni(int idx, int iris_pos, const float* lens_interfaces)
{
    return (idx == iris_pos) ? 1.0f : lens_interfaces[3 * idx + 1];
}
inline float effective_Ri(int idx, int iris_pos, const float* lens_interfaces)
{
    return (idx == iris_pos) ? INFINITY : lens_interfaces[3 * idx + 2];
}

// computeMs: Propagation starting from the iris aperture.
//
// C++ equivalent (simplified):
//   glm::mat2x2 Ms = I;
//   for (int i=m_iris_aperture_pos; i < num_interfaces; i++) {
//     if (i == m_iris_aperture_pos)
//       Ms = getTranslationRefractionMatrix(lens_interfaces[i].di, 1.f, effective_ni(i), effective_Ri(i)) * Ms;
//     else
//       Ms = getTranslationRefractionMatrix(lens_interfaces[i].di, effective_ni(i-1), effective_ni(i), effective_Ri(i)) * Ms;
//   }
inline void computeMs(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    float Ms[4])
{
    Ms[0] = 1.0f; Ms[1] = 0.0f; Ms[2] = 0.0f; Ms[3] = 1.0f;
    for (int i = iris_pos; i < num_interfaces; i++) {
        float T[4];
        if (i == iris_pos) {
            getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                1.0f,
                effective_ni(i, iris_pos, lens_interfaces),
                effective_Ri(i, iris_pos, lens_interfaces),
                T);
        }
        else {
            getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                effective_ni(i - 1, iris_pos, lens_interfaces),
                effective_ni(i, iris_pos, lens_interfaces),
                effective_Ri(i, iris_pos, lens_interfaces),
                T);
        }
        float temp[4];
        mat2_mul(T, Ms, temp);
        Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
    }
}

//---------------------------------------------------------------------
// Reflection variants:
// The following is a simplified conversion of your getMa(with reflections) method.
// It assumes that reflection positions (firstReflectionPos and secondReflectionPos) 
// are passed as integers and that they occur before the iris aperture.
// (If not, the function falls back to the no–reflection version.)
inline void computeMa_reflection(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    int firstReflectionPos,
    int secondReflectionPos,
    float Ma[4])
{
    // For simplicity, check that reflections happen before the iris.
    if (firstReflectionPos < iris_pos && secondReflectionPos < iris_pos &&
        firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0) {
        // Initialize Ma as identity.
        Ma[0] = 1.0f; Ma[1] = 0.0f; Ma[2] = 0.0f; Ma[3] = 1.0f;
        // Propagate from 0 to firstReflectionPos.
        for (int i = 0; i < firstReflectionPos; i++) {
            float T[4];
            if (i == 0) {
                getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                    1.0f,
                    lens_interfaces[3 * i + 1],
                    lens_interfaces[3 * i + 2],
                    T);
            }
            else {
                getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                    lens_interfaces[3 * (i - 1) + 1],
                    lens_interfaces[3 * i + 1],
                    lens_interfaces[3 * i + 2],
                    T);
            }
            float temp[4];
            mat2_mul(T, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        // Reflection at firstReflectionPos.
        {
            float Rf[4];
            getReflectionMatrix(lens_interfaces[3 * firstReflectionPos + 2], Rf);
            float temp[4];
            mat2_mul(Rf, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        // Back-propagate from firstReflectionPos-1 downto secondReflectionPos+1.
        for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--) {
            float T_inv[4];
            // Use the inverse propagation helper.
            getinverseRefractionBackwardsTranslationMatrix(lens_interfaces[3 * i + 0],
                lens_interfaces[3 * (i - 1) + 1],
                lens_interfaces[3 * i + 1],
                lens_interfaces[3 * i + 2],
                T_inv);
            float temp[4];
            mat2_mul(T_inv, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        // Apply translation and second reflection.
        {
            float T_sec[4];
            getTranslationMatrix(lens_interfaces[3 * secondReflectionPos + 0], T_sec);
            float temp[4];
            mat2_mul(T_sec, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        {
            float Rf2[4];
            getReflectionMatrix(-lens_interfaces[3 * secondReflectionPos + 2], Rf2);
            float temp[4];
            mat2_mul(Rf2, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        {
            float T_sec[4];
            getTranslationMatrix(lens_interfaces[3 * secondReflectionPos + 0], T_sec);
            float temp[4];
            mat2_mul(T_sec, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
        // Finally, propagate from secondReflectionPos+1 to iris_pos.
        for (int i = secondReflectionPos + 1; i < iris_pos; i++) {
            float T[4];
            getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                lens_interfaces[3 * (i - 1) + 1],
                lens_interfaces[3 * i + 1],
                lens_interfaces[3 * i + 2],
                T);
            float temp[4];
            mat2_mul(T, Ma, temp);
            Ma[0] = temp[0]; Ma[1] = temp[1]; Ma[2] = temp[2]; Ma[3] = temp[3];
        }
    }
    else {
        // If the reflection positions aren’t valid (or not pre–aperture), fall back to the normal propagation.
        computeMa(lens_interfaces, num_interfaces, iris_pos, Ma);
    }
}

//---------------------------------------------------------------------
// computeMs_reflection: Compute the 2×2 system matrix (Ms)
// for propagation starting at the iris aperture with a reflection sequence.
// The reflection positions (firstReflectionPos and secondReflectionPos) should
// satisfy: firstReflectionPos > secondReflectionPos, secondReflectionPos >= iris_pos,
// and (ideally) both reflections occur after the iris aperture.
// If these conditions are not met, the function falls back to normal propagation.
inline void computeMs_reflection(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    int firstReflectionPos,
    int secondReflectionPos,
    float Ms[4])
{
    // Check that reflection indices are valid.
    if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= iris_pos && firstReflectionPos < num_interfaces)
    {
        // Apply reflection logic only if both reflections happen after the iris.
        if ((firstReflectionPos > iris_pos) && (secondReflectionPos > iris_pos))
        {
            // Initialize Ms as the identity matrix.
            Ms[0] = 1.0f; Ms[1] = 0.0f; Ms[2] = 0.0f; Ms[3] = 1.0f;

            // Forward propagation from iris_pos to firstReflectionPos (excluding the reflection interface).
            for (int i = iris_pos; i < firstReflectionPos; i++)
            {
                float T[4];
                if (i == iris_pos)
                {
                    getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                        1.0f,
                        effective_ni(i, iris_pos, lens_interfaces),
                        effective_Ri(i, iris_pos, lens_interfaces),
                        T);
                }
                else
                {
                    getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                        effective_ni(i - 1, iris_pos, lens_interfaces),
                        effective_ni(i, iris_pos, lens_interfaces),
                        effective_Ri(i, iris_pos, lens_interfaces),
                        T);
                }
                float temp[4];
                mat2_mul(T, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }

            // Reflection at firstReflectionPos.
            {
                float Rf[4];
                getReflectionMatrix(effective_Ri(firstReflectionPos, iris_pos, lens_interfaces), Rf);
                float temp[4];
                mat2_mul(Rf, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }

            // Backward propagation: from firstReflectionPos-1 down to secondReflectionPos+1.
            for (int i = firstReflectionPos - 1; i > secondReflectionPos; i--)
            {
                float T_inv[4];
                getinverseRefractionBackwardsTranslationMatrix(lens_interfaces[3 * i + 0],
                    effective_ni(i - 1, iris_pos, lens_interfaces),
                    effective_ni(i, iris_pos, lens_interfaces),
                    effective_Ri(i, iris_pos, lens_interfaces),
                    T_inv);
                float temp[4];
                mat2_mul(T_inv, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }

            // Second reflection handling:
            // First, apply a translation at secondReflectionPos.
            {
                float T_sec[4];
                getTranslationMatrix(lens_interfaces[3 * secondReflectionPos + 0], T_sec);
                float temp[4];
                mat2_mul(T_sec, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }
            // Now apply reflection with a negative curvature at secondReflectionPos.
            {
                float Rf2[4];
                getReflectionMatrix(-effective_Ri(secondReflectionPos, iris_pos, lens_interfaces), Rf2);
                float temp[4];
                mat2_mul(Rf2, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }
            // And apply another translation at secondReflectionPos.
            {
                float T_sec[4];
                getTranslationMatrix(lens_interfaces[3 * secondReflectionPos + 0], T_sec);
                float temp[4];
                mat2_mul(T_sec, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }

            // Finally, forward propagation after secondReflectionPos until the end of the system.
            for (int i = secondReflectionPos + 1; i < num_interfaces; i++)
            {
                float T[4];
                getTranslationRefractionMatrix(lens_interfaces[3 * i + 0],
                    effective_ni(i - 1, iris_pos, lens_interfaces),
                    effective_ni(i, iris_pos, lens_interfaces),
                    effective_Ri(i, iris_pos, lens_interfaces),
                    T);
                float temp[4];
                mat2_mul(T, Ms, temp);
                Ms[0] = temp[0]; Ms[1] = temp[1]; Ms[2] = temp[2]; Ms[3] = temp[3];
            }
        }
        else
        {
            // If both reflections do not occur after the iris, fall back to normal propagation.
            // (This uses the computeMs function defined elsewhere.)
            // For brevity, we call computeMs here.
            computeMs(lens_interfaces, num_interfaces, iris_pos, Ms);
        }
    }
    else
    {
        // If reflection positions are invalid, fall back.
        computeMs(lens_interfaces, num_interfaces, iris_pos, Ms);
    }
}

// Pre–aperture reflections: corresponds to getPreAptReflections()
// This function scans lens interfaces with indices in [0, iris_pos)
// and for each candidate pair (i, j) that meets the criteria, writes an int2 pair {i, j}.
// Parameters:
//   lens_interfaces : a pointer to a flat array storing (di, ni, Ri) for each interface.
//   num_interfaces  : total number of interfaces.
//   iris_pos        : the iris aperture position.
//   outReflections  : pointer to an output array of int2 (pre-allocated with enough space).
//   outCount        : pointer to an integer where the number of found pairs will be stored.
inline void getPreAptReflections(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    int2* outReflections,
    int* outCount)
{
    int count = 0;
    // Loop for interfaces before the iris aperture.
    for (int i = 1; i < iris_pos; i++) {
        // For each i, check lower indices j.
        for (int j = i - 1; j >= 0; j--) {
            // Get refractive index values for interface i and its predecessor.
            float ni_i = lens_interfaces[3 * i + 1];
            float ni_im1 = lens_interfaces[3 * (i - 1) + 1];
            if (ni_i > 1.1f || ni_im1 > 1.1f) {
                // Now check candidate j.
                if (j > 0) {
                    float ni_j = lens_interfaces[3 * j + 1];
                    float ni_jm1 = lens_interfaces[3 * (j - 1) + 1];
                    if (ni_j > 1.1f || ni_jm1 > 1.1f) {
                        outReflections[count] = (int2)(i, j);
                        count++;
                    }
                }
                else if (lens_interfaces[3 * j + 1] > 1.1f) {
                    outReflections[count] = (int2)(i, j);
                    count++;
                }
            }
        }
    }
    *outCount = count;
}

// Post–aperture reflections: corresponds to getPostAptReflections()
// This function scans lens interfaces with indices in (iris_pos, num_interfaces)
// and writes reflection pairs that meet the criteria to the outReflections array.
// Parameters are similar to the above.
inline void getPostAptReflections(const float* lens_interfaces,
    int num_interfaces,
    int iris_pos,
    int2* outReflections,
    int* outCount)
{
    int count = 0;
    // Loop for interfaces strictly after the iris aperture.
    for (int i = iris_pos + 2; i < num_interfaces; i++) {
        for (int j = i - 1; j > iris_pos; j--) {
            float ni_i = lens_interfaces[3 * i + 1];
            float ni_im1 = lens_interfaces[3 * (i - 1) + 1];
            if (ni_i > 1.1f || ni_im1 > 1.1f) {
                if (j > 0) {
                    float ni_j = lens_interfaces[3 * j + 1];
                    float ni_jm1 = lens_interfaces[3 * (j - 1) + 1];
                    if (ni_j > 1.1f || ni_jm1 > 1.1f) {
                        outReflections[count] = (int2)(i, j);
                        count++;
                    }
                }
                else if (lens_interfaces[3 * j + 1] > 1.1f) {
                    outReflections[count] = (int2)(i, j);
                    count++;
                }
            }
        }
    }
    *outCount = count;
}


// Helper function that calculates a "snapshot" (quad center position and height)
// based on the given matrices and parameters. The result is stored in the local array snap:
// snap[0] = quadCenterPos.x, snap[1] = quadCenterPos.y, snap[2] = quadHeight.
inline void simulateDrawQuad(const float Ma[4],
    const float Ms[4],
    float light_angle_x,
    float light_angle_y,
    float irisApertureHeight,
    float snap[3])
{
    // Compute ghost_center_x = (-light_angle_x * Ma[1][0] / Ma[0][0], light_angle_x)
    // Here, Ma[0] = a, Ma[1] = b (i.e. first column of Ma).
    float a = Ma[0];
    float b = Ma[2];
    float2 ghost_center_x = (float2)(-light_angle_x * b / a, light_angle_x);
    float2 ghost_center_y = (float2)(-light_angle_y * b / a, light_angle_y);

    // Transform the ghost centers:
    float2 tmp = mat2_mul_vec2(Ma, ghost_center_x);
    float2 ghost_center_x_s = mat2_mul_vec2(Ms, tmp);

    tmp = mat2_mul_vec2(Ma, ghost_center_y);
    float2 ghost_center_y_s = mat2_mul_vec2(Ms, tmp);

    // Define combined ghost center position as (x component from ghost_center_x_s, x from ghost_center_y_s)
    float2 ghost_center_pos = (float2)(ghost_center_x_s.x, ghost_center_y_s.x);

    // Compute apt_h_x = ( (irisApertureHeight/2 - light_angle_x * Ma[1][0]) / Ma[0][0], light_angle_x )
    float apt_h_x_first = (irisApertureHeight / 2.0f - light_angle_x * b) / a;
    float2 apt_h_x = (float2)(apt_h_x_first, light_angle_x);
    tmp = mat2_mul_vec2(Ma, apt_h_x);
    float2 apt_h_x_s = mat2_mul_vec2(Ms, tmp);
    float ghost_height = fabs(apt_h_x_s.x - ghost_center_pos.x);

    // Compute transformed entrance pupil parameters:
    // entrance_pupil_h_x_s = Ms * Ma * vec2(irisApertureHeight/2, light_angle_x)
    float2 entrance_pupil_h_x = (float2)(100.f / 2.0f, light_angle_x);
    tmp = mat2_mul_vec2(Ma, entrance_pupil_h_x);
    float2 entrance_pupil_h_x_s = mat2_mul_vec2(Ms, tmp);

    // entrance_pupil_center_x_s = Ms * Ma * vec2(0, light_angle_x)
    float2 v0_lightx = (float2)(0.0f, light_angle_x);
    tmp = mat2_mul_vec2(Ma, v0_lightx);
    float2 entrance_pupil_center_x_s = mat2_mul_vec2(Ms, tmp);

    // entrance_pupil_center_y_s = Ms * Ma * vec2(0, light_angle_y)
    float2 v0_lighty = (float2)(0.0f, light_angle_y);
    tmp = mat2_mul_vec2(Ma, v0_lighty);
    float2 entrance_pupil_center_y_s = mat2_mul_vec2(Ms, tmp);

    float entrance_pupil_height = fabs(entrance_pupil_h_x_s.x - entrance_pupil_center_x_s.x);
    float2 entrance_pupil_center_pos = (float2)(entrance_pupil_center_x_s.x, entrance_pupil_center_y_s.x);

    // Determine if the ghost center is "clipped"
    float2 diff = entrance_pupil_center_pos - ghost_center_pos;
    float dist_between_centers = sqrt(diff.x * diff.x + diff.y * diff.y);
    int ghost_center_clipped = ((entrance_pupil_height < dist_between_centers) &&
        (ghost_height < dist_between_centers)) ? 1 : 0;

    // Set the output snapshot
    if (ghost_center_clipped) {
        snap[0] = ghost_center_pos.x;
        snap[1] = ghost_center_pos.y;
        snap[2] = 100000.0f;  // will sort last due to high height
    }
    else if (entrance_pupil_height < ghost_height) {
        snap[0] = entrance_pupil_center_pos.x;
        snap[1] = entrance_pupil_center_pos.y;
        snap[2] = entrance_pupil_height;
    }
    else {
        snap[0] = ghost_center_pos.x;
        snap[1] = ghost_center_pos.y;
        snap[2] = ghost_height;
    }
}

//=====================================================================
// MAIN KERNEL
//=====================================================================
__kernel void batch_fitness_kernel(__global const double* d_population,
    __global double* d_fitness,
    __global const double* d_renderObj,
    const int candidate_dim,
    const int num_render_obj,
    const float light_angle_x,
    const float light_angle_y)
{
    int idx = get_global_id(0);
    const int base_index = idx * candidate_dim;
    float fitness_value = 0.0f;
    #define MAX_INTERFACES 40
    #define MAX_INTERFACEPARAMS (40 * 3)
    #define MAX_GHOSTS ((MAX_INTERFACES * (MAX_INTERFACES - 1)) / 2)


    // Reconstruct candidate parameters.
    // d_population[base_index + 0] = apt pos
    // d_population[base_index + 1] = apt height.
    int apt_pos = (int)round((float)d_population[base_index + 0]);
    float apt_height = (float)d_population[base_index + 1];
    // Number of interfaces reconstructed:
    int num_interfaces = (candidate_dim - 2) / 3;

    // Build candidate’s interface array (repaired) in a local array.
    float interface_params[MAX_INTERFACEPARAMS];
    for (int i = 0; i < num_interfaces; i++) {
        int param_base = base_index + 2 + i * 3;
        float di = (float)d_population[param_base + 0];
        float ni = (float)d_population[param_base + 1];
        float Ri = (float)d_population[param_base + 2];
        if (ni <= 1.25f)
            ni = 1.0f;  // Air gap
        else
            ni = ni + 0.25f; // Shift into typical glass range
        if (ni != 1.0f)
            di = 1.0f + ((di - 0.1f) / (100.0f - 0.1f)) * 9.0f;
        if (Ri >= 0.0f) {
            if (Ri < 5.0f) Ri = 5.0f;
            if (Ri > 8000.0f) Ri = INFINITY;
        }
        else {
            if (Ri > -5.0f) Ri = -5.0f;
            if (Ri < -8000.0f) Ri = -INFINITY;
        }
        interface_params[i * 3 + 0] = di;
        interface_params[i * 3 + 1] = ni;
        interface_params[i * 3 + 2] = Ri;
    }

    // --- Render Ghost Simulation ---
    int2 preAptPairs[MAX_GHOSTS];
    int preAptCount = 0;
    int2 postAptPairs[MAX_GHOSTS];
    int postAptCount = 0;

    // Get reflection pairs.
    // Note: lens_interfaces here refers to our candidate interfaces (interface_params).
    getPreAptReflections(interface_params, num_interfaces, apt_pos, preAptPairs, &preAptCount);
    getPostAptReflections(interface_params, num_interfaces, apt_pos, postAptPairs, &postAptCount);

    // If not enough ghost images are produced, assign a high penalty.
    if ((preAptCount + postAptCount) < num_render_obj) {
        fitness_value = 100000.0f;
        d_fitness[idx] = (double)fitness_value;
        return;
    }

    // Compute default system matrices without reflections.
    float default_Ma[4], default_Ms[4];
    computeMa(interface_params, num_interfaces, apt_pos, default_Ma);
    computeMs(interface_params, num_interfaces, apt_pos, default_Ms);

    // Compute reflection–based matrices for ghost images.

    float preAptMas[MAX_GHOSTS][4];
    for (int i = 0; i < preAptCount; i++) {
        int2 pair = preAptPairs[i];
        computeMa_reflection(interface_params, num_interfaces, apt_pos, pair.x, pair.y, preAptMas[i]);
    }
    float postAptMss[MAX_GHOSTS][4];
    for (int i = 0; i < postAptCount; i++) {
        int2 pair = postAptPairs[i];
        computeMs_reflection(interface_params, num_interfaces, apt_pos, pair.x, pair.y, postAptMss[i]);
    }

    // Create ghost snapshots.
    float snapshots[MAX_GHOSTS][3];
    int ghostCount = 0;
    // Pre–aperture ghosts: use the computed Ma (from reflections) with default_Ms.
    for (int i = 0; i < preAptCount; i++) {
        simulateDrawQuad(preAptMas[i], default_Ms, light_angle_x, light_angle_y, apt_height, snapshots[ghostCount]);
        ghostCount++;
    }
    // Post–aperture ghosts: use default_Ma with computed Ms from reflection.
    for (int i = 0; i < postAptCount; i++) {
        simulateDrawQuad(default_Ma, postAptMss[i], light_angle_x, light_angle_y, apt_height, snapshots[ghostCount]);
        ghostCount++;
    }

    // Sort ghosts by quad height (snapshot[2]) using a simple bubble sort.
    for (int i = 0; i < ghostCount - 1; i++) {
        for (int j = 0; j < ghostCount - i - 1; j++) {
            if (snapshots[j][2] > snapshots[j + 1][2]) {
                float t0 = snapshots[j][0];
                float t1 = snapshots[j][1];
                float t2 = snapshots[j][2];
                snapshots[j][0] = snapshots[j + 1][0];
                snapshots[j][1] = snapshots[j + 1][1];
                snapshots[j][2] = snapshots[j + 1][2];
                snapshots[j + 1][0] = t0;
                snapshots[j + 1][1] = t1;
                snapshots[j + 1][2] = t2;
            }
        }
    }

    // Compute fitness: compare ghost snapshots with render objective data in d_renderObj.
    float f = 0.0f;
    int num_render = num_render_obj;
    for (int i = 0; i < num_render; i++) {
        float target_pos_x = (float)d_renderObj[i * 3 + 0];
        float target_pos_y = (float)d_renderObj[i * 3 + 1];
        float target_size = (float)d_renderObj[i * 3 + 2];
        float dx = target_pos_x - snapshots[i][0];
        float dy = target_pos_y - snapshots[i][1];
        float posError = sqrt(dx * dx + dy * dy);
        float sizeError = target_size - snapshots[i][2];
        f += posError * posError + sizeError * sizeError;
    }
    if (ghostCount > num_render) {
        for (int i = num_render; i < ghostCount; i++) {
            f += 500.0f / snapshots[i][2];
        }
    }
    f = f / num_render;
    fitness_value = f;

    // Write fitness result.
    d_fitness[idx] = (double)fitness_value;
}

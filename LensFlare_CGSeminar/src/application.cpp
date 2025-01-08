// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
// Library for loading an image
#include <stb/stb_image.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include "line_drawer.h"
#include "lens_system.h"
#include "ray_propagation_drawer.h"
#include "quad.h"

//UTIL FUNCTIONS
float toRad(float degrees) {
    return degrees * 3.141593f / 180.0f;
}

LensSystem generateExampleLens() {

    std::vector<LensInterface> lensInterfaces;
    //                                     thickness, refractive index, radius, height
    lensInterfaces.push_back(LensInterface(7.7f,    1.652f,     30.81f,     14.5f)); //LAKN7
    lensInterfaces.push_back(LensInterface(1.85f,   1.603f,     -89.35f,    14.5f)); //F5
    lensInterfaces.push_back(LensInterface(3.52f,   1.f,       580.38f,     14.5f)); //air
    lensInterfaces.push_back(LensInterface(1.85f,   1.643f,     -80.63f,    12.3f)); //BAF9
    lensInterfaces.push_back(LensInterface(4.18f,   1.f,       28.34f,      12.f)); //air
    lensInterfaces.push_back(LensInterface(3.0f,    1.f,       std::numeric_limits<float>::infinity(), 11.6f)); //air (iris aperture)
    lensInterfaces.push_back(LensInterface(1.85f,   1.581f,     std::numeric_limits<float>::infinity(), 12.3f)); //LF5
    lensInterfaces.push_back(LensInterface(7.27f,   1.694f,     32.19f,     12.3f)); //LAK13
    lensInterfaces.push_back(LensInterface(81.857f, 1.f,       -52.99f,     12.3f)); //air

    return LensSystem(5, 10.f, lensInterfaces);

}

class Application {
public:
    Application()
        : m_window("Lens Flare Rendering", glm::ivec2(1080, 1080), OpenGLVersion::GL45)
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
            });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
            });

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

        }
        catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void update()
    {
        //INITIALIZATION

        /* Light Direction */
        float light_angle_x = 0.f;
        float light_angle_y = 0.f;

        /* Light Pos */
        float light_pos_x = 0.f;
        float light_pos_y = 0.f;
        float light_pos_z = 0.f;

        /* Texture */
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("resources/aperture.png", &texWidth, &texHeight, &texChannels, STBI_grey);

        if (!pixels) { std::cerr << "Failed to load texture" << std::endl; }

        GLuint texApt;
        glCreateTextures(GL_TEXTURE_2D, 1, &texApt);
        glTextureStorage2D(texApt, 1, GL_R8, texWidth, texHeight);
        glTextureSubImage2D(texApt, 0, 0, 0, texWidth, texHeight, GL_RED, GL_UNSIGNED_BYTE, pixels);

        // Set behaviour for when texture coordinates are outside the [0, 1] range.
        glTextureParameteri(texApt, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texApt, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // Set interpolation for texture sampling (GL_NEAREST for no interpolation).
        glTextureParameteri(texApt, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texApt, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(pixels);


        int interfaceToUpdate = 0;
        int interfaceToUpdatePreviousValue = -1;
        float newdi = 0.f;
        float newni = 0.f;
        float newRi = 0.f;
        float newhi = 0.f;

        int interfaceToRemove = 0;

        int firstReflectionPos = 0;
        int secondReflectionPos = 0;
        int firstReflectionPosMemory = -1;
        int secondReflectionPosMemory = -1;
        bool reflectionActive = false;
        bool reflectionActiveMemory = false;

        LensSystem lensSystem = generateExampleLens();
        int irisAperturePos = lensSystem.getIrisAperturePos();
        float sensorSize = lensSystem.getSensorSize();
        std::vector<LensInterface> lensInterfaces = lensSystem.getLensInterfaces();

        /* Flare Paths and Matrices */
        glm::mat2x2 default_Ma = lensSystem.getMa();
        glm::mat2x2 default_Ms = lensSystem.getMs();
        std::vector<glm::vec2> preAptReflectionPairs = lensSystem.getPreAptReflections();
        std::vector<glm::vec2> postAptReflectionPairs = lensSystem.getPostAptReflections();
        std::vector<glm::mat2x2> preAptMas = lensSystem.getMa(preAptReflectionPairs);
        std::vector<glm::mat2x2> postAptMss = lensSystem.getMs(postAptReflectionPairs);
        std::vector<FlareQuad> preAptQuads;
        std::vector<FlareQuad> postAptQuads;
        std::vector<glm::vec3> quad_points = {
                {7.5f, 7.5f, 0.0f},   //top right
                {7.5f, -7.5f, 0.0f},  //bottom right
                {-7.5f, -7.5f, 0.0f}, //bottom left
                {-7.5f, 7.5f, 0.0f}   //top left
        };
        for (int i = 0; i < preAptReflectionPairs.size(); i++) {
            preAptQuads.push_back(FlareQuad(quad_points));
        }
        for (int i = 0; i < postAptReflectionPairs.size(); i++) {
            postAptQuads.push_back(FlareQuad(quad_points));
        }
        /* */

        //LOOP
        while (!m_window.shouldClose()) {
            m_window.updateInput();

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            // https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            ImGui::Begin("Settings");
            //Input for Ray
            ImGui::InputFloat("Light Angle X", &light_angle_x);
            ImGui::InputFloat("Light Angle Y", &light_angle_y);
            ImGui::InputInt("Aperture Position", &irisAperturePos);
            ImGui::InputFloat("Sensor Size", &sensorSize);
            ImGui::InputInt("First Reflection Location", &firstReflectionPos);
            ImGui::InputInt("Second Reflection Location", &secondReflectionPos);
            ImGui::Checkbox("Toggle Reflection", &reflectionActive);

            //Button loading example lens system
            if (ImGui::Button("Load Example Lens System")) {
                lensSystem = generateExampleLens();
                irisAperturePos = lensSystem.getIrisAperturePos();
                sensorSize = lensSystem.getSensorSize();
                lensInterfaces = lensSystem.getLensInterfaces();
            }

            if (ImGui::CollapsingHeader("Modify Interface")) {
                if (ImGui::InputInt("Interface to Update", &interfaceToUpdate)) {
                    if (interfaceToUpdate != interfaceToUpdatePreviousValue) {
                        if (interfaceToUpdate < lensInterfaces.size() && interfaceToUpdate >= 0) {
                            newdi = lensInterfaces[interfaceToUpdate].di;
                            newni = lensInterfaces[interfaceToUpdate].ni;
                            newRi = lensInterfaces[interfaceToUpdate].Ri;
                            newhi = lensInterfaces[interfaceToUpdate].hi;
                        }
                        else {
                            newdi = 0.f;
                            newni = 0.f;
                            newRi = 0.f;
                            newhi = 0.f;
                        }
                        interfaceToUpdatePreviousValue = interfaceToUpdate;
                    }
                }

                ImGui::InputFloat("Thickness", &newdi);
                ImGui::InputFloat("Refractive Index", &newni);
                ImGui::InputFloat("Radius", &newRi);
                ImGui::InputFloat("Height", &newhi);
                if (ImGui::Button("Update")) {
                    LensInterface newLensInterface(newdi, newni, newRi, newhi);
                    if (interfaceToUpdate < lensInterfaces.size() && interfaceToUpdate >= 0) {
                        //update an existing interface
                        lensInterfaces[interfaceToUpdate] = newLensInterface;
                    }
                    else if (interfaceToUpdate < 0) {
                        //add at the front
                        lensInterfaces.insert(lensInterfaces.begin(), newLensInterface);
                    }
                    else {
                        //add at the back
                        lensInterfaces.push_back(newLensInterface);
                    }
                    lensSystem.setLensInterfaces(lensInterfaces);
                }
            }

            if (ImGui::CollapsingHeader("Remove Interface")) {
                ImGui::InputInt("Interface to Remove", &interfaceToRemove);
                if (ImGui::Button("Remove")) {
                    if (interfaceToRemove >= 0 && interfaceToRemove < lensInterfaces.size()) {
                        lensInterfaces.erase(lensInterfaces.begin() + interfaceToRemove);
                        lensSystem.setLensInterfaces(lensInterfaces);
                    }
                }
            }

            for (int i = 0; i < lensInterfaces.size(); i++) {
                std::string lensInterfaceDescription = "Interface " + std::to_string(i) + ", d = " + std::to_string(lensInterfaces[i].di) + ", n = " + std::to_string(lensInterfaces[i].ni) + ", R = " + std::to_string(lensInterfaces[i].Ri) + ", h = " + std::to_string(lensInterfaces[i].hi);
                ImGui::Text(lensInterfaceDescription.c_str());
            }

            ImGui::End();

            if (irisAperturePos >= lensInterfaces.size()) {
                irisAperturePos = lensInterfaces.size() - 1;
            }

            if (irisAperturePos < 0) {
                irisAperturePos = 0;
            }

            // Update Projection Matrix
            glm::mat4 projection = glm::ortho(m_left_clipping_plane, m_right_clipping_plane, m_bottom_clipping_plane, m_top_clipping_plane);

            //Update Iris Aperture Position
            lensSystem.setIrisAperturePos(irisAperturePos);
            lensSystem.setSensorSize(sensorSize);

            if ((!reflectionActiveMemory && reflectionActive) || reflectionActiveMemory && (firstReflectionPos != firstReflectionPosMemory || secondReflectionPos != secondReflectionPosMemory)) {
                if (firstReflectionPos > secondReflectionPos && secondReflectionPos >= 0 && firstReflectionPos < lensInterfaces.size()) {
                    reflectionActiveMemory = true;
                    firstReflectionPosMemory = firstReflectionPos;
                    secondReflectionPosMemory = secondReflectionPos;
                }
            }

            if (reflectionActiveMemory && !reflectionActive) {
                reflectionActiveMemory = false;
                firstReflectionPosMemory = -1;
                secondReflectionPosMemory = -1;
            }

            // Clear the screen
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            // Enable blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            
            //RENDERING
            m_defaultShader.bind();
            float entrancePupilHeight = lensInterfaces[0].hi / 2.f;
            if (reflectionActive) {
                for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                    preAptQuads[i].drawQuad(projection, preAptMas[i], default_Ms, glm::vec3(0.3f, 0.5f, 0.5f), texApt, toRad(light_angle_x), toRad(light_angle_y), entrancePupilHeight);
                }
                for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                    postAptQuads[i].drawQuad(projection, default_Ma, postAptMss[i], glm::vec3(0.3f, 0.5f, 0.5f), texApt, toRad(light_angle_x), toRad(light_angle_y), entrancePupilHeight);
                }


                //flare_quad.drawQuad(projection, Ma, Ms, glm::vec3(1.f, 1.f, 1.f), texApt, toRad(light_angle_x), toRad(light_angle_y));
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    int panAndZoomSensitivity = 4.f;
    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        switch (key)
        {
        case GLFW_KEY_A:
            m_left_clipping_plane -= panAndZoomSensitivity;
            m_right_clipping_plane -= panAndZoomSensitivity;
            break;
        case GLFW_KEY_D:
            m_left_clipping_plane += panAndZoomSensitivity;
            m_right_clipping_plane += panAndZoomSensitivity;
            break;
        case GLFW_KEY_S:
            m_bottom_clipping_plane -= panAndZoomSensitivity;
            m_top_clipping_plane -= panAndZoomSensitivity;
            break;
        case GLFW_KEY_W:
            m_bottom_clipping_plane += panAndZoomSensitivity;
            m_top_clipping_plane += panAndZoomSensitivity;
            break;
        case GLFW_KEY_Q:
            m_left_clipping_plane -= panAndZoomSensitivity;
            m_right_clipping_plane += panAndZoomSensitivity;
            m_bottom_clipping_plane -= panAndZoomSensitivity;
            m_top_clipping_plane += panAndZoomSensitivity;
            break;
        case GLFW_KEY_E:
            if (abs(m_left_clipping_plane - m_right_clipping_plane) > 2.f) {
                m_left_clipping_plane += panAndZoomSensitivity;
                m_right_clipping_plane -= panAndZoomSensitivity;
                m_bottom_clipping_plane += panAndZoomSensitivity;
                m_top_clipping_plane -= panAndZoomSensitivity;
            }
            break;
        default:
            break;
        }
        //std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        //std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        //std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        //std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        //std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    Window m_window;

    // Shader for default rendering
    Shader m_defaultShader;

    bool m_useMaterial{ false };

    float m_left_clipping_plane = -30.0f;
    float m_right_clipping_plane = 30.0f;
    float m_bottom_clipping_plane = -30.0f;
    float m_top_clipping_plane = 30.0f;
};

int main()
{
    Application app;
    app.update();

    return 0;
}

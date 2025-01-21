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
#include <framework/mesh.h>
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include "lens_system.h"
#include "quad.h"
#include "camera.h"
#include "utils.h"
#include "preset_lens_systems.h"

constexpr int WIDTH = 1920;
constexpr int HEIGHT = 1080;

class Application {
public:
    Application()
        : m_window("Lens Flare Rendering", glm::ivec2(WIDTH, HEIGHT), OpenGLVersion::GL45)
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

            m_lightShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, "shaders/light_vertex.glsl").addStage(GL_FRAGMENT_SHADER, "shaders/light_frag.glsl").build();

        }
        catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void update()
    {
        /* INIT */

        /* LIGHT SPHERE */
        const Mesh lightSphere = mergeMeshes(loadMesh("resources/sphere.obj"));
        GLuint ibo_light;
        glCreateBuffers(1, &ibo_light);
        glNamedBufferStorage(ibo_light, static_cast<GLsizeiptr>(lightSphere.triangles.size() * sizeof(decltype(Mesh::triangles)::value_type)), lightSphere.triangles.data(), 0);
        GLuint vbo_light;
        glCreateBuffers(1, &vbo_light);
        glNamedBufferStorage(vbo_light, static_cast<GLsizeiptr>(lightSphere.vertices.size() * sizeof(Vertex)), lightSphere.vertices.data(), 0);
        GLuint vao_light;
        glCreateVertexArrays(1, &vao_light);
        glVertexArrayElementBuffer(vao_light, ibo_light);
        glVertexArrayVertexBuffer(vao_light, 0, vbo_light, offsetof(Vertex, position), sizeof(Vertex));
        glEnableVertexArrayAttrib(vao_light, 0);

        /* Light POS */
        float light_pos_x = 0.001f;
        float light_pos_y = 0.001f;
        float light_pos_z = 25.f;

        /* Aperture Texture */
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

        /* GUI PARAMS */
        int interfaceToUpdate = 0;
        int interfaceToUpdatePreviousValue = -1;
        float newdi = 0.f;
        float newni = 0.f;
        float newRi = 0.f;
        float newhi = 0.f;
        float newlambda0 = 0.0f;

        int interfaceToRemove = 0;

        LensSystem lensSystem = someCanonLens();
        //LensSystem lensSystem = heliarTronerLens();
        int irisAperturePos = lensSystem.getIrisAperturePos();
        int irisAperturePosMemory = irisAperturePos;
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
        float ePHeight = (lensSystem.getEntrancePupilHeight() / 2);
        std::vector<glm::vec3> quad_points = {
                {ePHeight, ePHeight, 0.0f},   //top right
                {ePHeight, -ePHeight, 0.0f},  //bottom right
                {-ePHeight, -ePHeight, 0.0f}, //bottom left
                {-ePHeight, ePHeight, 0.0f}   //top left
        };
        int quad_id = 0;
        for (int i = 0; i < preAptReflectionPairs.size(); i++) {
            preAptQuads.push_back(FlareQuad(quad_points, quad_id));
            quad_id++;
        }
        for (int i = 0; i < postAptReflectionPairs.size(); i++) {
            postAptQuads.push_back(FlareQuad(quad_points, quad_id));
            quad_id++;
        }
        /* */

        //LOOP
        while (!m_window.shouldClose()) {
            m_window.updateInput();
            m_camera.updateInput();

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            // https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            ImGui::Begin("Settings");
            //Input for Ray
            ImGui::InputFloat("Light Pos X", &light_pos_x);
            ImGui::InputFloat("Light Pos Y", &light_pos_y);
            ImGui::InputFloat("Light Pos Z", &light_pos_z);
            ImGui::InputInt("Aperture Position", &irisAperturePos);
            ImGui::InputFloat("Sensor Size", &sensorSize);

            //Button loading example lens system
            if (ImGui::Button("Load Example Lens System")) {
                lensSystem = heliarTronerLens();
                irisAperturePos = lensSystem.getIrisAperturePos();
                sensorSize = lensSystem.getSensorSize();
                lensInterfaces = lensSystem.getLensInterfaces();
                /* Flare Paths and Matrices */
                default_Ma = lensSystem.getMa();
                default_Ms = lensSystem.getMs();
                preAptReflectionPairs = lensSystem.getPreAptReflections();
                postAptReflectionPairs = lensSystem.getPostAptReflections();
                preAptMas = lensSystem.getMa(preAptReflectionPairs);
                postAptMss = lensSystem.getMs(postAptReflectionPairs);
                preAptQuads.clear();
                postAptQuads.clear();
                quad_id = 0;
                for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                    preAptQuads.push_back(FlareQuad(quad_points, quad_id));
                    quad_id++;
                }
                for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                    postAptQuads.push_back(FlareQuad(quad_points, quad_id));
                    quad_id++;
                }
                /* */
            }

            if (ImGui::CollapsingHeader("Modify Interface")) {
                if (ImGui::InputInt("Interface to Update", &interfaceToUpdate)) {
                    if (interfaceToUpdate != interfaceToUpdatePreviousValue) {
                        if (interfaceToUpdate < lensInterfaces.size() && interfaceToUpdate >= 0) {
                            newdi = lensInterfaces[interfaceToUpdate].di;
                            newni = lensInterfaces[interfaceToUpdate].ni;
                            newRi = lensInterfaces[interfaceToUpdate].Ri;
                            newhi = lensInterfaces[interfaceToUpdate].hi;
                            newlambda0 = lensInterfaces[interfaceToUpdate].lambda0;
                        }
                        else {
                            newdi = 0.f;
                            newni = 0.f;
                            newRi = 0.f;
                            newhi = 0.f;
                            newlambda0 = 0.f;
                        }
                        interfaceToUpdatePreviousValue = interfaceToUpdate;
                    }
                }

                ImGui::InputFloat("Thickness", &newdi);
                ImGui::InputFloat("Refractive Index", &newni);
                ImGui::InputFloat("Radius", &newRi);
                ImGui::InputFloat("Height", &newhi);
                ImGui::InputFloat("Lambda0", &newlambda0);
                if (ImGui::Button("Update")) {
                    LensInterface newLensInterface(newdi, newni, newRi, newhi, newlambda0);
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

                    /* Flare Paths and Matrices */
                    default_Ma = lensSystem.getMa();
                    default_Ms = lensSystem.getMs();
                    preAptReflectionPairs = lensSystem.getPreAptReflections();
                    postAptReflectionPairs = lensSystem.getPostAptReflections();
                    preAptMas = lensSystem.getMa(preAptReflectionPairs);
                    postAptMss = lensSystem.getMs(postAptReflectionPairs);
                    preAptQuads.clear();
                    postAptQuads.clear();
                    quad_id = 0;
                    for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                        preAptQuads.push_back(FlareQuad(quad_points, quad_id));
                        quad_id++;
                    }
                    for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                        postAptQuads.push_back(FlareQuad(quad_points, quad_id));
                        quad_id++;
                    }
                    /* */

                }
            }

            if (ImGui::CollapsingHeader("Remove Interface")) {
                ImGui::InputInt("Interface to Remove", &interfaceToRemove);
                if (ImGui::Button("Remove")) {
                    if (interfaceToRemove >= 0 && interfaceToRemove < lensInterfaces.size()) {
                        lensInterfaces.erase(lensInterfaces.begin() + interfaceToRemove);
                        lensSystem.setLensInterfaces(lensInterfaces);
                        /* Flare Paths and Matrices */
                        default_Ma = lensSystem.getMa();
                        default_Ms = lensSystem.getMs();
                        preAptReflectionPairs = lensSystem.getPreAptReflections();
                        postAptReflectionPairs = lensSystem.getPostAptReflections();
                        preAptMas = lensSystem.getMa(preAptReflectionPairs);
                        postAptMss = lensSystem.getMs(postAptReflectionPairs);
                        preAptQuads.clear();
                        postAptQuads.clear();
                        quad_id = 0;
                        for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                            preAptQuads.push_back(FlareQuad(quad_points, quad_id));
                            quad_id++;
                        }
                        for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                            postAptQuads.push_back(FlareQuad(quad_points, quad_id));
                            quad_id++;
                        }
                        /* */
                    }
                }
            }

            for (int i = 0; i < lensInterfaces.size(); i++) {
                std::string lensInterfaceDescription = "Interface " + std::to_string(i) + ", d = " + std::to_string(lensInterfaces[i].di) + ", n = " + std::to_string(lensInterfaces[i].ni) + ", R = " + std::to_string(lensInterfaces[i].Ri) + ", h = " + std::to_string(lensInterfaces[i].hi) + ", lambda0 = " + std::to_string(lensInterfaces[i].lambda0);
                ImGui::Text(lensInterfaceDescription.c_str());
            }

            ImGui::End();

            if (irisAperturePos >= lensInterfaces.size()) {
                irisAperturePos = lensInterfaces.size() - 1;
            }

            if (irisAperturePos < 0) {
                irisAperturePos = 0;
            }

            //Update Iris Aperture Position
            if (irisAperturePos != irisAperturePosMemory) {
                lensSystem.setIrisAperturePos(irisAperturePos);
                irisAperturePosMemory = irisAperturePos;
                /* Flare Paths and Matrices */
                default_Ma = lensSystem.getMa();
                default_Ms = lensSystem.getMs();
                preAptReflectionPairs = lensSystem.getPreAptReflections();
                postAptReflectionPairs = lensSystem.getPostAptReflections();
                preAptMas = lensSystem.getMa(preAptReflectionPairs);
                postAptMss = lensSystem.getMs(postAptReflectionPairs);
                preAptQuads.clear();
                postAptQuads.clear();
                quad_id = 0;
                for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                    preAptQuads.push_back(FlareQuad(quad_points, quad_id));
                    quad_id++;
                }
                for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                    postAptQuads.push_back(FlareQuad(quad_points, quad_id));
                    quad_id++;
                }
                /* */
            }
            
            lensSystem.setSensorSize(sensorSize);

            // Update Projection Matrix
            const glm::mat4 mvp = m_mainProjectionMatrix * m_camera.viewMatrix();
            const glm::vec3 cameraPos = m_camera.cameraPos();
            const glm::vec3 cameraForward = m_camera.m_forward;
            const glm::vec3 cameraUp = m_camera.m_up;
            const glm::vec3 lightPos = {light_pos_x, light_pos_y, light_pos_z};
            glm::vec2 yawandPitch = getYawandPitch(cameraPos, cameraForward, cameraUp, lightPos);
            glm::vec2 cameraYawandPitch = m_camera.getYawAndPitch();
            float irisApertureHeight = lensSystem.getIrisApertureHeight();
            float entrancePupilHeight = lensSystem.getEntrancePupilHeight() / 2.f;

            glm::mat4 sensorMatrix = glm::translate(glm::mat4(1.0f), cameraPos);
            sensorMatrix = glm::rotate(sensorMatrix, cameraYawandPitch.x, glm::vec3(0.0f, 1.0f, 0.0f));
            sensorMatrix = glm::rotate(sensorMatrix, cameraYawandPitch.y, glm::vec3(1.0f, 0.0f, 0.0f));

            std::vector<glm::vec3> preAPTtransmissions = lensSystem.getTransmission(preAptReflectionPairs, glm::vec2(0.0f, yawandPitch.x));
            std::vector<glm::vec3> postAPTtransmissions = lensSystem.getTransmission(postAptReflectionPairs, glm::vec2(0.0f, yawandPitch.x));

            // Clear the screen
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            //glEnable(GL_DEPTH_TEST);

            // Enable blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            
            //RENDERING
            m_defaultShader.bind();
            for (int i = 0; i < preAptReflectionPairs.size(); i++) {
                preAptQuads[i].drawQuad(mvp, preAptMas[i], default_Ms, glm::vec3(100.0f, 100.0f, 100.0f) * preAPTtransmissions[i], texApt, yawandPitch, entrancePupilHeight, sensorMatrix, irisApertureHeight);
            }
            for (int i = 0; i < postAptReflectionPairs.size(); i++) {
                postAptQuads[i].drawQuad(mvp, default_Ma, postAptMss[i], glm::vec3(100.0f, 100.0f, 100.0f)* postAPTtransmissions[i], texApt, yawandPitch, entrancePupilHeight, sensorMatrix, irisApertureHeight);
            }


            //RENDER LIGHT SOURCE
            m_lightShader.bind();
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(mvp));;
            glUniform3fv(1, 1, glm::value_ptr(m_lcolor));
            glUniform3fv(2, 1, glm::value_ptr(lightPos));
            glBindVertexArray(vao_light);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>((lightSphere.triangles.size() * 3)), GL_UNSIGNED_INT, nullptr);


            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    int panAndZoomSensitivity = 3.f;
    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        switch (key)
        {
        /*case GLFW_KEY_A:
            break;
        */
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

    /* Camera Stuff */
    Camera m_camera{&m_window, glm::vec3(0.0f, 0.0f, -1.0f), -glm::vec3(0.0f, 0.0f, -1.0f)};
    const float m_fov = glm::radians(70.0f);
    //float m_visibleWidth = 0.14f;
    float m_distance = 0.5f;
    //// Calculate the FOV in radians
    //float m_fov = 2.0f * atan(m_visibleWidth / (2.0f * m_distance));
    const float m_aspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
    const glm::mat4 m_mainProjectionMatrix = glm::perspective(m_fov, m_aspect, 0.01f, 100.0f);

    // Shader for default rendering
    Shader m_defaultShader;
    Shader m_lightShader;
    const glm::vec3 m_lcolor{ 1, 1, 0.5 };

    bool m_useMaterial{ false };

};

int main()
{
    Application app;
    app.update();

    return 0;
}

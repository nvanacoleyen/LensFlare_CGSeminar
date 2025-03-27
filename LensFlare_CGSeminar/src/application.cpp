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
#include "starburst.h"
#include "reverse_coating.h"
#include "Windows.h"
#include "lens_solver.h"

/* GLOBAL PARAMS */
HWND hwnd = GetConsoleWindow();
UINT dpi = GetDpiForWindow(hwnd);
const float DISPLAY_SCALING_FACTOR = static_cast<float>(dpi) / 96.0f;
const char* APERTURE_TEXTURE = "resources/aperture.png";

class Application {
public:
    Application()
        : m_window("Lens Flare Rendering", glm::ivec2(1920 / DISPLAY_SCALING_FACTOR, 1080 / DISPLAY_SCALING_FACTOR), OpenGLVersion::GL45)
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
        const glm::ivec2 &window_size = m_window.getWindowSize();
        glViewport(window_size.x/4, 0, window_size.x -(window_size.x /4), window_size.y);
        m_window.registerWindowResizeCallback([this](const glm::ivec2& size) {
            glViewport(size.x / 4, 0, size.x - (size.x / 4), size.y);
            m_camera.setLeftSideIgnore(size.x / 4);
            m_aspect = static_cast<float>(size.x - (size.x / 4)) / static_cast<float>(size.y);
            m_mainProjectionMatrix = glm::perspective(m_fov, m_aspect, 0.01f, 100.0f);
            });

        /* Camera Params */
        m_camera.setLeftSideIgnore(window_size.x / 4);
        m_fov = glm::radians(50.0f);
        m_distance = 0.5f;
        m_aspect = static_cast<float>(window_size.x - (window_size.x / 4)) / static_cast<float>(window_size.y);
        m_mainProjectionMatrix = glm::perspective(m_fov, m_aspect, 0.01f, 100.0f);

        /* Build Shaders */
        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();
            m_starburstShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, "shaders/starburst_vertex.glsl").addStage(GL_FRAGMENT_SHADER, "shaders/starburst_frag.glsl").build();
            m_lightShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, "shaders/light_vertex.glsl").addStage(GL_FRAGMENT_SHADER, "shaders/light_frag.glsl").build();
            m_quadCenterShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, "shaders/quadcenter_vert.glsl").addStage(GL_FRAGMENT_SHADER, "shaders/quadcenter_frag.glsl").build();

        }
        catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    /* Method to update matrices and quads whenever there's a lens system change */
    void refreshMatricesAndQuads() {
        m_default_Ma = m_lensSystem.getMa();
        m_default_Ms = m_lensSystem.getMs();

        m_preAptReflectionPairs = m_lensSystem.getPreAptReflections();
        m_postAptReflectionPairs = m_lensSystem.getPostAptReflections();

        for (FlareQuad &flareQuad : m_preAptQuads) {
            flareQuad.releaseArrayAndBuffer();
        }
        for (FlareQuad& flareQuad : m_postAptQuads) {
            flareQuad.releaseArrayAndBuffer();
        }

        m_preAptQuads.clear();
        m_postAptQuads.clear();
        
        m_preAptMas = m_lensSystem.getMa(m_preAptReflectionPairs);
        m_postAptMss = m_lensSystem.getMs(m_postAptReflectionPairs);
        float ePHeight = (m_lensSystem.getEntrancePupilHeight() / 2);
        std::vector<glm::vec3> quad_points = {
                {ePHeight, ePHeight, 0.0f},   //top right
                {ePHeight, -ePHeight, 0.0f},  //bottom right
                {-ePHeight, -ePHeight, 0.0f}, //bottom left
                {-ePHeight, ePHeight, 0.0f}   //top left
        };
        int quad_id = 0;
        for (int i = 0; i < m_preAptReflectionPairs.size(); i++) {
            m_preAptQuads.push_back(FlareQuad(quad_points, quad_id));
            quad_id++;
        }
        for (int i = 0; i < m_postAptReflectionPairs.size(); i++) {
            m_postAptQuads.push_back(FlareQuad(quad_points, quad_id));
            quad_id++;
        }
        m_resetAnnotations = true;
    }

    void update()
    {
        /* INIT */

        /* Create starburst texture */
        //createStarburst(APERTURE_TEXTURE);

        /* Light Sphere */
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
        float light_pos_x = 0.0001f;
        float light_pos_y = 0.0001f;
        float light_pos_z = 25.f;

        /* Aperture Texture */
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(APERTURE_TEXTURE, &texWidth, &texHeight, &texChannels, STBI_grey);

        if (!pixels) { std::cerr << "Failed to load aperture texture" << std::endl; }

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

        /* Starburst */
        pixels = stbi_load("resources/starburst_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            std::cerr << "Failed to load starburst texture" << std::endl;
        }

        GLuint texStarburst;
        glCreateTextures(GL_TEXTURE_2D, 1, &texStarburst);
        glTextureStorage2D(texStarburst, 1, GL_RGBA8, texWidth, texHeight);
        glTextureSubImage2D(texStarburst, 0, 0, 0, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // Set behaviour for when texture coordinates are outside the [0, 1] range.
        glTextureParameteri(texStarburst, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(texStarburst, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // Set interpolation for texture sampling (GL_NEAREST for no interpolation).
        glTextureParameteri(texStarburst, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texStarburst, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(pixels);

        float starburstData[] = {
            // positions        // texture coords
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,

            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f
        };

        GLuint vao_starburst;
        glGenVertexArrays(1, &vao_starburst);
        glBindVertexArray(vao_starburst);

        GLuint vbo_starburst;
        glGenBuffers(1, &vbo_starburst);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_starburst);
        glBufferData(GL_ARRAY_BUFFER, sizeof(starburstData), starburstData, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind the VBO
        glBindVertexArray(0); // Unbind the VAO

        //QUADCENTERS

        GLuint vbo_quadcenter;
        glGenBuffers(1, &vbo_quadcenter);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_quadcenter);
        glBufferData(GL_ARRAY_BUFFER, m_quadcenter_points.size() * sizeof(glm::vec2), m_quadcenter_points.data(), GL_STATIC_DRAW);

        GLuint vao_quadcenter;
        glGenVertexArrays(1, &vao_quadcenter);
        glBindVertexArray(vao_quadcenter);

        // Enable and define the vertex attribute for position
        glEnableVertexAttribArray(0); // Use location=0 in your shader
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);

        // Unbind the VAO to prevent accidental modifications
        glBindVertexArray(0);


        /* SHADER BUFFERS */
        m_defaultShader.bind();
        const GLuint MAX_BUFFER_SIZE = 10000; // Maximum number of quad data entries to store
        GLuint ssboQuadData;
        glGenBuffers(1, &ssboQuadData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboQuadData);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_BUFFER_SIZE * sizeof(QuadData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboQuadData);

        GLuint atomicCounterBuffer;
        glGenBuffers(1, &atomicCounterBuffer);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, atomicCounterBuffer);

        GLuint ssboSnapshotData;
        glGenBuffers(1, &ssboSnapshotData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSnapshotData);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_BUFFER_SIZE * sizeof(SnapshotData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboSnapshotData);

        GLuint atomicCounterBufferSnapshot;
        glGenBuffers(1, &atomicCounterBufferSnapshot);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBufferSnapshot);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, atomicCounterBufferSnapshot);


        /* GUI PARAMS */
        int interfaceToUpdate = 0;
        int interfaceToUpdatePreviousValue = -1;
        float newdi = 0.f;
        float newni = 0.f;
        float newRi = 0.f;
        float newhi = 0.f;
        float newlambda0 = 0.0f;

        int interfaceToRemove = 0;
        
        int irisAperturePos = m_lensSystem.getIrisAperturePos();
        int irisAperturePosMemory = irisAperturePos;

        glm::vec3 selected_ghost_color = glm::vec3(1.0f, 0.0f, 0.0f);

        bool highlightSelectedQuad = true;
        int selectedQuadId = -1;
        int selectedQuadIdMemory = -1;
        glm::vec2 selectedQuadReflectionInterfaces;
        int firstQuadReflectionInterface = -1;
        int secondQuadReflectionInterface = -1;
        float firstQuadReflectionInterfaceLambda0 = 0.0;
        float secondQuadReflectionInterfaceLambda0 = 0.0;

        bool optimizeCoatings = false;
        bool lensInterfaceRefresh = false;
        bool optimizeWithEA = false;


        /* Flare Paths and Matrices */
        std::vector<LensInterface> lensInterfaces = m_lensSystem.getLensInterfaces();
        refreshMatricesAndQuads();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

        /* RENDER LOOP */
        while (!m_window.shouldClose()) {
            m_window.updateInput();
            m_camera.updateInput();

            ImGui::SetNextWindowPos(ImVec2(0, 0)); // Position at the top-left corner
            const glm::ivec2& window_size = m_window.getWindowSize();
            ImGui::SetNextWindowSize(ImVec2(window_size.x / 4, window_size.y)); // Fixed width, full height

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            // https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            ImGui::Begin("Settings", NULL, window_flags);
            //Input for Ray
            //ImGui::InputFloat("Light Pos X", &light_pos_x);
            //ImGui::InputFloat("Light Pos Y", &light_pos_y);
            //ImGui::InputFloat("Light Pos Z", &light_pos_z);
            ImGui::InputInt("Aperture Position", &irisAperturePos);

            //Button loading example lens system
            if (ImGui::Button("Load Example Lens System")) {
                //m_lensSystem = heliarTronerLens();
                m_lensSystem = someCanonLens();
                irisAperturePos = m_lensSystem.getIrisAperturePos();
                lensInterfaces = m_lensSystem.getLensInterfaces();
                refreshMatricesAndQuads();
            }

            if (ImGui::CollapsingHeader("Lens Prescription Details")) {
                for (int i = 0; i < lensInterfaces.size(); i++) {
                std::string lensInterfaceDescription = std::to_string(i) + ": d=" + std::format("{:.3f}", lensInterfaces[i].di) + ", n=" + std::format("{:.3f}", lensInterfaces[i].ni) + ", R=" + std::format("{:.3f}", lensInterfaces[i].Ri) + ", h=" + std::format("{:.3f}", lensInterfaces[i].hi) + ", lambda0=" + std::format("{:.3f}", lensInterfaces[i].lambda0);
                ImGui::Text(lensInterfaceDescription.c_str());
                }
            }

            if (ImGui::CollapsingHeader("Modify Interface")) {
                ImGui::InputInt("Interface to Update", &interfaceToUpdate);
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
                    m_lensSystem.setLensInterfaces(lensInterfaces);
                    refreshMatricesAndQuads();

                }
            }

            if (ImGui::CollapsingHeader("Remove Interface")) {
                ImGui::InputInt("Interface to Remove", &interfaceToRemove);
                if (ImGui::Button("Remove")) {
                    if (interfaceToRemove >= 0 && interfaceToRemove < lensInterfaces.size()) {
                        lensInterfaces.erase(lensInterfaces.begin() + interfaceToRemove);
                        m_lensSystem.setLensInterfaces(lensInterfaces);
                        refreshMatricesAndQuads();
                    }
                }
            }

            ImGui::ColorEdit3("Ghost Selection Color", (float*)&selected_ghost_color);

            if (ImGui::Button("Optimize Coatings")) {
                optimizeCoatings = true;
            }

            if (ImGui::Button("Optimize with EA (DE)")) {
                m_takeSnapshot = true;
                optimizeWithEA = true;
            }

            if (ImGui::Button("Reset Annotations")) {
                m_resetAnnotations = true;
            }

            if (m_selectedQuadIndex != -1) {
                //get pairs of the id reflection
                selectedQuadId = m_selectedQuadIDs[m_selectedQuadIndex];
                if (selectedQuadId != selectedQuadIdMemory || lensInterfaceRefresh == true) {
                    //update vals
                    if (selectedQuadId < m_preAptReflectionPairs.size()) {
                        selectedQuadReflectionInterfaces = m_preAptReflectionPairs[selectedQuadId];
                    }
                    else {
                        selectedQuadReflectionInterfaces = m_postAptReflectionPairs[selectedQuadId - m_preAptReflectionPairs.size()];
                    }
                    firstQuadReflectionInterface = selectedQuadReflectionInterfaces[0];
                    secondQuadReflectionInterface = selectedQuadReflectionInterfaces[1];
                    firstQuadReflectionInterfaceLambda0 = lensInterfaces[firstQuadReflectionInterface].lambda0;
                    secondQuadReflectionInterfaceLambda0 = lensInterfaces[secondQuadReflectionInterface].lambda0;
                    selectedQuadIdMemory = selectedQuadId;
                    lensInterfaceRefresh = false;
                }
             
                ImGui::Checkbox("Highlight Quad", &highlightSelectedQuad);
                std::string label_lambda = "Lambda0 of Interface ";
                ImGui::Text((label_lambda + std::to_string(firstQuadReflectionInterface)).c_str());
                ImGui::InputFloat("(1)", &firstQuadReflectionInterfaceLambda0);
                ImGui::Text((label_lambda + std::to_string(secondQuadReflectionInterface)).c_str());
                ImGui::InputFloat("(2)", &secondQuadReflectionInterfaceLambda0);
                if (ImGui::Button("Apply Changes")) {
                    lensInterfaces[firstQuadReflectionInterface].lambda0 = firstQuadReflectionInterfaceLambda0;
                    lensInterfaces[secondQuadReflectionInterface].lambda0 = secondQuadReflectionInterfaceLambda0;
                    m_lensSystem.setLensInterfaces(lensInterfaces);
                }
            } else {
                selectedQuadId = -1;
                selectedQuadIdMemory = -1;
                firstQuadReflectionInterface = -1;
                secondQuadReflectionInterface = -1;
                firstQuadReflectionInterfaceLambda0 = 0.0;
                secondQuadReflectionInterfaceLambda0 = 0.0;
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
                m_lensSystem.setIrisAperturePos(irisAperturePos);
                irisAperturePosMemory = irisAperturePos;
                refreshMatricesAndQuads();
            }

            // Reset annotations
            if (m_resetAnnotations) {
                m_annotationData.clear();
                int amount_ghosts = m_preAptReflectionPairs.size() + m_postAptReflectionPairs.size();
                for (int i = 0; i < amount_ghosts; i++) {
                    m_annotationData.push_back(AnnotationData());
                }
                m_resetAnnotations = false;
            }

            // Update Projection Matrix
            m_mvp = m_mainProjectionMatrix * m_camera.viewMatrix();
            const glm::vec3 cameraPos = m_camera.cameraPos();
            const glm::vec3 cameraForward = m_camera.m_forward;
            const glm::vec3 cameraUp = m_camera.m_up;
            const glm::vec3 lightPos = {light_pos_x, light_pos_y, light_pos_z};
            glm::vec2 yawandPitch = getYawandPitch(cameraPos, cameraForward, cameraUp, lightPos);
            glm::vec2 cameraYawandPitch = m_camera.getYawAndPitch();
            float irisApertureHeight = m_lensSystem.getIrisApertureHeight();
            float entrancePupilHeight = m_lensSystem.getEntrancePupilHeight() / 2.f;

            m_sensorMatrix = glm::translate(glm::mat4(1.0f), cameraPos);
            m_sensorMatrix = glm::rotate(m_sensorMatrix, cameraYawandPitch.x, glm::vec3(0.0f, 1.0f, 0.0f));
            m_sensorMatrix = glm::rotate(m_sensorMatrix, cameraYawandPitch.y, glm::vec3(1.0f, 0.0f, 0.0f));

            if (optimizeCoatings && m_selectedQuadIndex != -1) {
                if (m_selectedQuadIDs[m_selectedQuadIndex] < m_preAptReflectionPairs.size()) {
                    optimizeLensCoatingsBruteForce2(m_lensSystem, selected_ghost_color, m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]], yawandPitch);
                    //optimizeLensCoatingsSimple(m_lensSystem, ghost_color, m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]]);
                }
                else {
                    optimizeLensCoatingsBruteForce2(m_lensSystem, selected_ghost_color, m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()], yawandPitch);
                    //optimizeLensCoatingsSimple(m_lensSystem, ghost_color, m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()]);
                }
                lensInterfaces = m_lensSystem.getLensInterfaces();
                lensInterfaceRefresh = true;
                optimizeCoatings = false;
            }

            std::vector<glm::vec3> preAPTtransmissions = m_lensSystem.getTransmission(m_preAptReflectionPairs, yawandPitch);
            std::vector<glm::vec3> postAPTtransmissions = m_lensSystem.getTransmission(m_postAptReflectionPairs, yawandPitch);

            glm::vec2 cursorPos = m_window.getCursorPos();

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

            /* Bind General Variables */
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp)); // Projection Matrix
            glUniform1f(4, yawandPitch.x);
            glUniform1f(5, yawandPitch.y);
            glUniform1f(6, entrancePupilHeight);
            glUniformMatrix4fv(8, 1, GL_FALSE, glm::value_ptr(m_sensorMatrix));
            glUniform1f(9, irisApertureHeight);

            glUniform1i(10, cursorPos.x);
            glUniform1i(11, cursorPos.y);
            glUniform1i(13, m_getGhostsAtMouse ? 1 : 0);
            glUniform1i(14, m_takeSnapshot ? 1 : 0);
      
            glActiveTexture(GL_TEXTURE0); // Bind texture
            glBindTexture(GL_TEXTURE_2D, texApt);
            glUniform1i(7, 0);

            glm::vec3 light_intensity(50.0f);
            /* Bind Quad Specific Variables */
            for (int i = 0; i < m_preAptReflectionPairs.size(); i++) {
                if (m_selectedQuadIndex != -1 && m_selectedQuadIDs[m_selectedQuadIndex] == i && highlightSelectedQuad) {
                    m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, selected_ghost_color, m_annotationData[i]);
                }
                else {
                    glm::vec3 ghost_color = light_intensity * preAPTtransmissions[i];
                    m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, ghost_color, m_annotationData[i]);
                }
            }
            for (int i = 0; i < m_postAptReflectionPairs.size(); i++) {
                if (m_selectedQuadIndex != -1 && m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size() == i && highlightSelectedQuad) {
                    m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], selected_ghost_color, m_annotationData[i + m_preAptReflectionPairs.size()]);
                }
                else {
                    glm::vec3 ghost_color = light_intensity * postAPTtransmissions[i];
                    m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], ghost_color, m_annotationData[i + m_preAptReflectionPairs.size()]);
                }
            }

            if (m_getGhostsAtMouse) {
                GLuint numQuadsAtPixel = 0;
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
                glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &numQuadsAtPixel);

                m_selectedQuadIDs.clear();
                m_selectedQuadIDs.reserve(numQuadsAtPixel);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboQuadData);
                QuadData* ptr = (QuadData*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

                // Create a vector to hold the QuadData entries
                std::vector<QuadData> quadDataEntries(ptr, ptr + numQuadsAtPixel);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

                std::sort(quadDataEntries.begin(), quadDataEntries.end(), [](const QuadData& a, const QuadData& b) {
                    return a.intensityVal > b.intensityVal;
                    });

                // Extract the quadIDs from the sorted QuadData entries
                for (const auto& data : quadDataEntries) {
                    m_selectedQuadIDs.push_back(data.quadID);
                }

                //for (const auto& id : m_selectedQuadIDs) {
                //    std::cout << "Quad ID " << id << " rendered at the target pixel." << std::endl;
                //}

                if (!m_selectedQuadIDs.empty()) {
                    m_selectedQuadIndex = 0;
                    std::cout << "Selected Ghost: " << m_selectedQuadIDs[m_selectedQuadIndex] << std::endl;
                    if (m_selectedQuadIDs[m_selectedQuadIndex] < m_preAptReflectionPairs.size()) {
                        std::cout << "Reflections at: " << m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]].x << ", " << m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]].y << std::endl;
                    }
                    else {
                        
                        std::cout << "Reflections at: " << m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()].x << ", " << m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()].y << std::endl;
                    }

                    m_quadcenter_points.clear();

                    for (const auto& data : m_snapshotData) {
                        if (m_selectedQuadIDs[m_selectedQuadIndex] == data.quadID) {
                            m_quadcenter_points.push_back(data.quadCenterPos);
                        }
                    }

                    glBindBuffer(GL_ARRAY_BUFFER, vbo_quadcenter);
                    glBufferData(GL_ARRAY_BUFFER, m_quadcenter_points.size() * sizeof(glm::vec2), m_quadcenter_points.data(), GL_STATIC_DRAW);


                }
                else {
                    m_selectedQuadIndex = -1;
                }

                m_getGhostsAtMouse = false;
            }

            //take snapshot of the current state of the rendering
            if (m_takeSnapshot) {
                GLuint quadSnapshotCount = 0;
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBufferSnapshot);
                glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &quadSnapshotCount);

                m_snapshotData.clear();
                m_snapshotData.reserve(quadSnapshotCount);

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboSnapshotData);
                SnapshotData* ptr = (SnapshotData*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);

                for (GLuint i = 0; i < quadSnapshotCount; ++i) {
                    bool exists = false;
                    for (const auto& data : m_snapshotData) {
                        if (data.quadID == ptr[i].quadID) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        m_snapshotData.push_back(ptr[i]);
                    }
                }

                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

                m_takeSnapshot = false;
            }

            if (optimizeWithEA) {
                //Optimize
                m_lensSystem = solve_Annotations(m_lensSystem, m_snapshotData, yawandPitch.x, yawandPitch.y);
                lensInterfaces = m_lensSystem.getLensInterfaces();

                refreshMatricesAndQuads();

                irisAperturePos = m_lensSystem.getIrisAperturePos();
                irisAperturePosMemory = irisAperturePos;

                optimizeWithEA = false;
                m_resetAnnotations = true;
            }



            m_quadCenterShader.bind();
            glPointSize(5.0f); // Sets the size of points in pixels
            glm::vec3 quadCenterColor = glm::vec3(1.0, 0, 0);
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp));
            glUniform3fv(1, 1, glm::value_ptr(quadCenterColor));
            glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(m_sensorMatrix));

            glBindVertexArray(vao_quadcenter);
            glDrawArrays(GL_POINTS, 0, m_quadcenter_points.size());
            glBindVertexArray(0);



            //RENDER LIGHT SOURCE
            m_lightShader.bind();
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp));
            glUniform3fv(1, 1, glm::value_ptr(m_lcolor));
            glUniform3fv(2, 1, glm::value_ptr(lightPos));
            glBindVertexArray(vao_light);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>((lightSphere.triangles.size() * 3)), GL_UNSIGNED_INT, nullptr);

            //RENDER STARBURST
            glm::mat4 starburstMatrix = glm::translate(glm::mat4(1.0f), lightPos);
            starburstMatrix = glm::rotate(starburstMatrix, cameraYawandPitch.x, glm::vec3(0.0f, 1.0f, 0.0f));
            starburstMatrix = glm::rotate(starburstMatrix, cameraYawandPitch.y, glm::vec3(1.0f, 0.0f, 0.0f));

            glm::vec3 starburstColor = glm::vec3({ 50.f, 50.f, 50.f });
            float starburstScale = 25;

            m_starburstShader.bind();
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp));
            glUniform3fv(1, 1, glm::value_ptr(starburstColor));
            glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(starburstMatrix));
            glUniform1f(3, starburstScale);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texStarburst);
            glUniform1i(4, 0);
            glBindVertexArray(vao_starburst);
            glDrawArrays(GL_TRIANGLES, 0, 6);


            //Reset atomic counter
            GLuint zero = 0;
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
            glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBufferSnapshot);
            glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

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
        case GLFW_KEY_BACKSPACE:
            m_selectedQuadIDs.clear();
            m_selectedQuadIndex = -1;
            break;
        case GLFW_KEY_UP:
            if (m_selectedQuadIndex != -1 && m_selectedQuadIndex < m_selectedQuadIDs.size() - 1) {
                m_selectedQuadIndex++;
                std::cout << "Selected Ghost: " << m_selectedQuadIDs[m_selectedQuadIndex] << std::endl;
            }
            break;
        case GLFW_KEY_DOWN:
            if (m_selectedQuadIndex != -1 && m_selectedQuadIndex > 0) {
                m_selectedQuadIndex--;
                std::cout << "Selected Ghost: " << m_selectedQuadIDs[m_selectedQuadIndex] << std::endl;
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
        switch (key)
        {
        case GLFW_KEY_W:
        case GLFW_KEY_A:
        case GLFW_KEY_S:
        case GLFW_KEY_D:
            m_takeSnapshot = true;
            break;
        default:
            break;
        }
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        //std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
        if (m_window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) { //Very optimizable
            if (m_window.isKeyPressed(GLFW_KEY_Q)) { // Q for moving the selected ghost
                if (m_selectedQuadIndex != -1) {
                    glm::vec4 quadCenterScreenPos;
                    for (SnapshotData& snapshotdata : m_snapshotData) {
                        if (snapshotdata.quadID == m_selectedQuadIDs[m_selectedQuadIndex]) {
                            quadCenterScreenPos = m_mvp * m_sensorMatrix * glm::vec4(snapshotdata.quadCenterPos, 30.0, 1.0);
                        }
                    }
                    quadCenterScreenPos = quadCenterScreenPos / quadCenterScreenPos.w;
                    const glm::ivec2& window_size = m_window.getWindowSize();
                    glm::vec2 mycursorpos = cursorPos;
                    mycursorpos = (mycursorpos - glm::vec2(window_size.x / 4.f + (((3.f * window_size.x) / 4.f) / 2.f), window_size.y / 2.f)) / glm::vec2(((3.f * window_size.x) / 4.f) / 2.f, window_size.y / 2.f); //NDC
                    //std::cout << "gl quad center screen pos: " << quadCenterScreenPos.x << ", " << quadCenterScreenPos.y << std::endl; 
                    //std::cout << "window cursor pos: " << mycursorpos.x << ", " << mycursorpos.y << std::endl;
                    glm::vec2 diffVectorNDC = mycursorpos - glm::vec2(quadCenterScreenPos.x, quadCenterScreenPos.y);
                    glm::vec4 worldSpaceVector = glm::inverse(m_sensorMatrix) * glm::inverse(m_mvp) * glm::vec4(diffVectorNDC, quadCenterScreenPos.z, 1.f);
                    worldSpaceVector = worldSpaceVector / worldSpaceVector.w;
                    m_annotationData[m_selectedQuadIDs[m_selectedQuadIndex]].posAnnotationTransform = glm::vec2(worldSpaceVector.x, worldSpaceVector.y);
                }
            }
            else if (m_window.isKeyPressed(GLFW_KEY_E)) { // E for scaling the selected ghost
                if (m_selectedQuadIndex != -1) {
                    glm::vec2 currentCursorPos = cursorPos;
                    glm::vec2 cursorResizeVector = currentCursorPos - m_resizeInitialPos;
                    //std::cout << "CURSOR AT :" << currentCursorPos.x << ", " << currentCursorPos.y << std::endl;
                    float resizeWeight = sqrt(pow(cursorResizeVector.x, 2.f) + pow(cursorResizeVector.y, 2.f)) / (float) m_window.getWindowSize().y;
                    //std::cout << "WEIGHT : " << resizeWeight << std::endl;
                    if (currentCursorPos.x > m_resizeInitialPos.x) {
                        m_annotationData[m_selectedQuadIDs[m_selectedQuadIndex]].sizeAnnotationTransform += resizeWeight;
                    }
                    else if (m_annotationData[m_selectedQuadIDs[m_selectedQuadIndex]].sizeAnnotationTransform - resizeWeight > 0.0) {
                        m_annotationData[m_selectedQuadIDs[m_selectedQuadIndex]].sizeAnnotationTransform -= resizeWeight;
                    }
                    
                }
            }
        }
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            if (!m_window.isKeyPressed(GLFW_KEY_Q) && !m_window.isKeyPressed(GLFW_KEY_E)) {
                m_getGhostsAtMouse = true;
            }
            else if (m_window.isKeyPressed(GLFW_KEY_E)) {
                m_resizeInitialPos = m_window.getCursorPos();
                //std::cout << "INITIAL POINT AT:" << m_resizeInitialPos.x << ", " << m_resizeInitialPos.y << std::endl;
            }
            break;
        default:
            break;
        }
        //std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_RIGHT:
            m_takeSnapshot = true;
            break;
        default:
            break;
        }
        //std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    Window m_window;

    /* Camera Params */
    Camera m_camera{ &m_window, glm::vec3(0.0f, 0.0f, -1.0f), -glm::vec3(0.0f, 0.0f, -1.0f), 0 };
    float m_fov;
    float m_distance;
    float m_aspect;
    glm::mat4 m_mainProjectionMatrix;
    glm::mat4 m_mvp;

    /* Lens System */
    //LensSystem m_lensSystem = testLens();
    //LensSystem m_lensSystem = someCanonLens();
    LensSystem m_lensSystem = heliarTronerLens();
    glm::mat2x2 m_default_Ma = m_lensSystem.getMa();
    glm::mat2x2 m_default_Ms = m_lensSystem.getMs();
    std::vector<glm::vec2> m_preAptReflectionPairs = m_lensSystem.getPreAptReflections();
    std::vector<glm::vec2> m_postAptReflectionPairs = m_lensSystem.getPostAptReflections();
    std::vector<glm::mat2x2> m_preAptMas = m_lensSystem.getMa(m_preAptReflectionPairs);
    std::vector<glm::mat2x2> m_postAptMss = m_lensSystem.getMs(m_postAptReflectionPairs);
    std::vector<FlareQuad> m_preAptQuads;
    std::vector<FlareQuad> m_postAptQuads;

    glm::mat4 m_sensorMatrix;

    /* Ghost Selection */
    bool m_getGhostsAtMouse = false;
    std::vector<int> m_selectedQuadIDs;
    int m_selectedQuadIndex = -1;

    /* Annotations */
    bool m_resetAnnotations = true;
    std::vector<AnnotationData> m_annotationData;
    bool m_takeSnapshot = true;
    std::vector<SnapshotData> m_snapshotData;
    std::vector<glm::vec2> m_quadcenter_points;
    glm::vec2 m_resizeInitialPos;

    /* Shaders */
    Shader m_defaultShader;
    Shader m_lightShader;
    Shader m_starburstShader;
    Shader m_quadCenterShader;

    /* Light Source */
    const glm::vec3 m_lcolor{ 1, 1, 0.5 };

};

int main()
{
    Application app;
    app.update();

    return 0;
}

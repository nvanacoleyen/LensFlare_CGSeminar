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
const char* APERTURE_TEXTURE = "resources/aperture2.png";

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
            m_builderShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, "shaders/build_vert.glsl").addStage(GL_FRAGMENT_SHADER, "shaders/build_frag.glsl").build();

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
        float ePHeight = 3 * (m_lensSystem.getEntrancePupilHeight() / 2);
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
        m_calibrateLightSource = true;
    }

    void update()
    {
        /* INIT */

        /* Create starburst texture */
        createStarburst(APERTURE_TEXTURE);

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
        m_builderShader.bind();
        const GLuint MAX_BUFFER_SIZE = 10000; // Maximum number of quad data entries to store
        GLuint build_ssboQuadData;
        glGenBuffers(1, &build_ssboQuadData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, build_ssboQuadData);
        glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_BUFFER_SIZE * sizeof(QuadData), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, build_ssboQuadData);

        GLuint build_atomicCounterBuffer;
        glGenBuffers(1, &build_atomicCounterBuffer);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, build_atomicCounterBuffer);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 5, build_atomicCounterBuffer);

        m_defaultShader.bind();
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
		float min_light_intensity = 1.0f;
		float max_light_intensity = 10000.0f;

        int interfaceToUpdate = 0;
        int interfaceToUpdatePreviousValue = -1;
        float newdi = 0.f;
        float newni = 0.f;
        float newRi = 0.f;
        float newlambda0 = 0.0f;

        int interfaceToRemove = 0;
        
        int irisAperturePos = m_lensSystem.getIrisAperturePos();
        int irisAperturePosMemory = irisAperturePos;

        glm::vec3 selected_ghost_color = glm::vec3(10.0f, 0.0f, 0.0f);

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
        bool disableEntranceClipping = false;

        /* Flare Paths and Matrices */
        m_lens_interfaces = m_lensSystem.getLensInterfaces();
        refreshMatricesAndQuads();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

        /* RENDER LOOP */
        while (!m_window.shouldClose()) {
            m_window.updateInput();
            m_camera.updateInput();

            ImGui::SetNextWindowPos(ImVec2(0, 0)); // Position at the top-left corner
            const glm::ivec2& window_size = m_window.getWindowSize();
            ImGui::SetNextWindowSize(ImVec2(window_size.x / 4, window_size.y));

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            // https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
            ImGui::Begin("Settings", NULL, window_flags);

			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Light Settings");

            ImGui::SliderFloat("Light Intensity", &m_light_intensity, min_light_intensity, max_light_intensity);
            //TODO: ADD LIGHT COLOR

            if (!m_buildFromScratch) {
                //Button loading lens system
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Preset Lens Systems");
                if (ImGui::Button("Load Heliar Troner Lens System")) {
                    m_lensSystem = heliarTronerLens();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;
                }
                if (ImGui::Button("Load Canon Lens System")) {
                    m_lensSystem = someCanonLens();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;
                }
                if (ImGui::Button("Load Test Lens System")) {
                    m_lensSystem = testLens();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;
                }
            }


            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Global Lens Settings");
            ImGui::SliderFloat("Iris Aperture Height", &m_lensSystem.m_aperture_height, 0.0f, 40.0f);
            if (!m_buildFromScratch) {
                ImGui::InputInt("Aperture Position", &irisAperturePos);
                ImGui::SliderFloat("Entrance Pupil Height", &m_lensSystem.m_entrance_pupil_height, 0.0f, 40.0f);
                ImGui::Checkbox("Disable Entrance Clipping", &disableEntranceClipping);
            
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lens Interface Settings");
                if (ImGui::CollapsingHeader("Lens Prescription Details")) {
                    for (int i = 0; i < m_lens_interfaces.size(); i++) {
                    std::string lensInterfaceDescription = std::to_string(i) + ": d=" + std::format("{:.3f}", m_lens_interfaces[i].di) + ", n=" + std::format("{:.3f}", m_lens_interfaces[i].ni) + ", R=" + std::format("{:.3f}", m_lens_interfaces[i].Ri) + ", lambda0=" + std::format("{:.3f}", m_lens_interfaces[i].lambda0);
                    ImGui::Text(lensInterfaceDescription.c_str());
                    }
                }

                if (ImGui::CollapsingHeader("Modify Interface")) {
                    ImGui::InputInt("Interface to Update", &interfaceToUpdate);
                        if (interfaceToUpdate != interfaceToUpdatePreviousValue) {
                            if (interfaceToUpdate < m_lens_interfaces.size() && interfaceToUpdate >= 0) {
                                newdi = m_lens_interfaces[interfaceToUpdate].di;
                                newni = m_lens_interfaces[interfaceToUpdate].ni;
                                newRi = m_lens_interfaces[interfaceToUpdate].Ri;
                                newlambda0 = m_lens_interfaces[interfaceToUpdate].lambda0;
                            }
                            else {
                                    newdi = 0.f;
                                    newni = 0.f;
                                    newRi = 0.f;
                                    newlambda0 = 0.f;
                            }
                        interfaceToUpdatePreviousValue = interfaceToUpdate;
                        }

                    ImGui::InputFloat("Thickness", &newdi);
                    ImGui::InputFloat("Refractive Index", &newni);
                    ImGui::InputFloat("Radius", &newRi);
                    ImGui::InputFloat("Lambda0", &newlambda0);
                    if (ImGui::Button("Update")) {
                        LensInterface newLensInterface(newdi, newni, newRi, newlambda0);
                        if (interfaceToUpdate < m_lens_interfaces.size() && interfaceToUpdate >= 0) {
                            //update an existing interface
                            m_lens_interfaces[interfaceToUpdate] = newLensInterface;
                        }
                        else if (interfaceToUpdate < 0) {
                            //add at the front
                            m_lens_interfaces.insert(m_lens_interfaces.begin(), newLensInterface);
                        }
                        else {
                            //add at the back
                            m_lens_interfaces.push_back(newLensInterface);
                        }
                        m_lensSystem.setLensInterfaces(m_lens_interfaces);
                        refreshMatricesAndQuads();

                    }
                }

                if (ImGui::CollapsingHeader("Remove Interface")) {
                    ImGui::InputInt("Interface to Remove", &interfaceToRemove);
                    if (ImGui::Button("Remove")) {
                        if (interfaceToRemove >= 0 && interfaceToRemove < m_lens_interfaces.size()) {
                            m_lens_interfaces.erase(m_lens_interfaces.begin() + interfaceToRemove);
                            m_lensSystem.setLensInterfaces(m_lens_interfaces);
                            refreshMatricesAndQuads();
                        }
                    }
                }

                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Ghost Settings");
                ImGui::ColorEdit3("Ghost Color", (float*)&selected_ghost_color);
                ImGui::Checkbox("Highlight Quad", &highlightSelectedQuad);

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
                        firstQuadReflectionInterfaceLambda0 = m_lens_interfaces[firstQuadReflectionInterface].lambda0;
                        secondQuadReflectionInterfaceLambda0 = m_lens_interfaces[secondQuadReflectionInterface].lambda0;
                        selectedQuadIdMemory = selectedQuadId;
                        lensInterfaceRefresh = false;
                    }

				    ImGui::Text("Modify Size and Location:");
				    //TODO: Add toggle for changing first or second interface in the reflection pair
                    ImGui::Text("Change Thickness (d) with A");
                    ImGui::Text("Change Refractive Index (n) with S");
                    ImGui::Text("Change Radius (R) with D");
                    std::string lensInterfaceDescription = std::to_string(secondQuadReflectionInterface) + ": d=" + std::format("{:.3f}", m_lens_interfaces[secondQuadReflectionInterface].di) + ", n=" + std::format("{:.3f}", m_lens_interfaces[secondQuadReflectionInterface].ni) + ", R=" + std::format("{:.3f}", m_lens_interfaces[secondQuadReflectionInterface].Ri);
                    ImGui::Text(lensInterfaceDescription.c_str());

                    ImGui::Text("Modify Color:");
                    std::string label_lambda = "Lambda0 of Interface ";
                    ImGui::Text((label_lambda + std::to_string(firstQuadReflectionInterface)).c_str());
                    ImGui::InputFloat("(1)", &firstQuadReflectionInterfaceLambda0);
                    ImGui::Text((label_lambda + std::to_string(secondQuadReflectionInterface)).c_str());
                    ImGui::InputFloat("(2)", &secondQuadReflectionInterfaceLambda0);
                    if (ImGui::Button("Apply Changes")) {
                        m_lens_interfaces[firstQuadReflectionInterface].lambda0 = firstQuadReflectionInterfaceLambda0;
                        m_lens_interfaces[secondQuadReflectionInterface].lambda0 = secondQuadReflectionInterfaceLambda0;
                        m_lensSystem.setLensInterfaces(m_lens_interfaces);
                    }

                    if (ImGui::Button("Optimize Selected Ghost Color with Brute Force Search")) {
                        optimizeCoatings = true;
                    }


                }
                else {
                    selectedQuadId = -1;
                    selectedQuadIdMemory = -1;
                    firstQuadReflectionInterface = -1;
                    secondQuadReflectionInterface = -1;
                    firstQuadReflectionInterfaceLambda0 = 0.0;
                    secondQuadReflectionInterfaceLambda0 = 0.0;
                }
            
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Global Lens Optimization");
                if (ImGui::Button("Optimize Lens System with EA")) {
                    m_takeSnapshot = 2;
                    optimizeWithEA = true;
                }

                if (ImGui::Button("Reset Annotations")) {
                    m_resetAnnotations = true;
                }
            }

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Build from Scratch");
            if (m_buildFromScratch) {
                if (ImGui::Button("Abort")) {
                    //reset params
                    m_selectedQuadIndex = -1;
                    m_buildFromScratch = false;
                    m_resetAnnotations = true;

                }
                if (ImGui::Button("Add Ghost")) {
                    float ePHeight = 3 * (20.0f / 2);
                    std::vector<glm::vec3> quad_points = {
                            {ePHeight, ePHeight, 0.0f},   //top right
                            {ePHeight, -ePHeight, 0.0f},  //bottom right
                            {-ePHeight, -ePHeight, 0.0f}, //bottom left
                            {-ePHeight, ePHeight, 0.0f}   //top left
                    };
                    m_lens_builder_quads.push_back(FlareQuad(quad_points, m_buildQuadIDCounter));
                    m_buildQuadIDCounter++;
                    std::cout << "Ghost added" << std::endl;

                    m_annotationData.push_back(AnnotationData());

                }
                if (m_selectedQuadIndex != -1) {
                    if (ImGui::Button("Remove Selected Ghost")) {
                        int selectedQuadIndex = -1;
                        for (int i = 0; i < m_lens_builder_quads.size(); i++) {
                            if (m_lens_builder_quads[i].getID() == m_selectedQuadIDs[m_selectedQuadIndex]) {
                                selectedQuadIndex = i;
                                break;
                            }
                        }
                        if (selectedQuadIndex != -1) {
                            m_lens_builder_quads.erase(m_lens_builder_quads.begin() + selectedQuadIndex);
                            m_annotationData.erase(m_annotationData.begin() + selectedQuadIndex);
                            m_selectedQuadIndex = -1;
                            std::cout << "Ghost removed" << std::endl;
                        }
                    }
                }
                if (ImGui::Button("Reset Annotations")) {
                    m_resetAnnotations = true;
				}
				if (ImGui::Button("Build with EA")) {
                    //RUN EA and reset params
                    optimizeWithEA = true;
				}

            }
            else {
                if (ImGui::Button("Start")) {
                    m_buildFromScratch = true;
                    m_buildQuadIDCounter = 0;
                    m_selectedQuadIndex = -1;
                    m_resetAnnotations = true;
                    m_lens_builder_quads.clear();
                    m_light_intensity = 100.f;
					min_light_intensity = 1.0f;
                    max_light_intensity = 200.f;
                }
            }

            ImGui::End();

            if (irisAperturePos >= m_lens_interfaces.size()) {
                irisAperturePos = m_lens_interfaces.size() - 1;
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
				int amount_ghosts = 0;
                if (!m_buildFromScratch) {
                    amount_ghosts = m_preAptReflectionPairs.size() + m_postAptReflectionPairs.size();
                }
                else {
                    amount_ghosts = m_lens_builder_quads.size();
                }
                for (int i = 0; i < amount_ghosts; i++) {
                    m_annotationData.push_back(AnnotationData());
                }
                m_resetAnnotations = false;
            }

            glm::vec2 cursorPos = m_window.getCursorPos();

            // Update Projection Matrix
            m_mvp = m_mainProjectionMatrix * m_camera.viewMatrix();
            const glm::vec3 cameraPos = m_camera.cameraPos();
            const glm::vec3 cameraForward = m_camera.m_forward;
            const glm::vec3 cameraUp = m_camera.m_up;
            glm::vec2 yawandPitch = getYawandPitch(cameraPos, cameraForward, cameraUp, m_light_pos);
            glm::vec2 cameraYawandPitch = m_camera.getYawAndPitch();
            float irisApertureHeight = m_lensSystem.getApertureHeight();
            float entrancePupilHeight = m_lensSystem.getEntrancePupilHeight() / 2.f;

            m_sensorMatrix = glm::translate(glm::mat4(1.0f), cameraPos);
            m_sensorMatrix = glm::rotate(m_sensorMatrix, cameraYawandPitch.x, glm::vec3(0.0f, 1.0f, 0.0f));
            m_sensorMatrix = glm::rotate(m_sensorMatrix, cameraYawandPitch.y, glm::vec3(1.0f, 0.0f, 0.0f));

            // Clear the screen
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Enable blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            if (!m_buildFromScratch) {

                if (optimizeCoatings && m_selectedQuadIndex != -1) {
                    if (m_selectedQuadIDs[m_selectedQuadIndex] < m_preAptReflectionPairs.size()) {
                        optimizeLensCoatingsBruteForce(m_lensSystem, selected_ghost_color, m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]], yawandPitch);
                    }
                    else {
                        optimizeLensCoatingsBruteForce(m_lensSystem, selected_ghost_color, m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()], yawandPitch);
                    }
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    lensInterfaceRefresh = true;
                    optimizeCoatings = false;
                }


                glm::vec2 post_apt_center_ray_x = glm::vec2(-yawandPitch.x / 10 * m_default_Ma[1][0] / m_default_Ma[0][0], yawandPitch.x / 10);
                glm::vec2 post_apt_center_ray_y = glm::vec2(yawandPitch.y / 10 * m_default_Ma[1][0] / m_default_Ma[0][0], -yawandPitch.y / 10);
                std::vector<glm::vec2> pre_apt_center_ray_x;
                std::vector<glm::vec2> pre_apt_center_ray_y;
                for (auto& const preAptMa : m_preAptMas) {
                    pre_apt_center_ray_x.push_back(glm::vec2(-yawandPitch.x / 10 * preAptMa[1][0] / preAptMa[0][0], yawandPitch.x / 10));
                    pre_apt_center_ray_y.push_back(glm::vec2(yawandPitch.y / 10 * preAptMa[1][0] / preAptMa[0][0], -yawandPitch.y / 10));
                }
                std::vector<glm::vec3> preAPTtransmissions = m_lensSystem.getTransmission(m_preAptReflectionPairs, pre_apt_center_ray_x, pre_apt_center_ray_y);
                std::vector<glm::vec3> postAPTtransmissions = m_lensSystem.getTransmission(m_postAptReflectionPairs, post_apt_center_ray_x, post_apt_center_ray_y);

                if (m_calibrateLightSource) {
                    double totalSum = 0.0;
                    //int totalCount = 0;

                    // Process preAPTtransmissions
                    for (const auto& vec : preAPTtransmissions) {
                        totalSum += vec.x + vec.y + vec.z;
                        //totalCount += 3;
                    }

                    // Process postAPTtransmissions
                    for (const auto& vec : postAPTtransmissions) {
                        totalSum += vec.x + vec.y + vec.z;
                        //totalCount += 3;
                    }

					m_light_intensity = (1.0f / (totalSum)) * 20;
					min_light_intensity = m_light_intensity * 0.1f;
					max_light_intensity = m_light_intensity * 5.0f;
					m_calibrateLightSource = false;
                }

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
                glUniform1i(14, m_takeSnapshot);
                glUniform1i(17, disableEntranceClipping ? 1 : 0);

                glActiveTexture(GL_TEXTURE0); // Bind texture
                glBindTexture(GL_TEXTURE_2D, texApt);
                glUniform1i(7, 0);

                /* Bind Quad Specific Variables */
                for (int i = 0; i < m_preAptReflectionPairs.size(); i++) {
                    if (m_selectedQuadIndex != -1 && m_selectedQuadIDs[m_selectedQuadIndex] == i && highlightSelectedQuad) {
                        m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, selected_ghost_color, m_annotationData[i]);
                    }
                    else {
                        glm::vec3 ghost_color = m_light_intensity * preAPTtransmissions[i];
                        m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, ghost_color, m_annotationData[i]);
                    }
                }
                for (int i = 0; i < m_postAptReflectionPairs.size(); i++) {
                    if (m_selectedQuadIndex != -1 && m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size() == i && highlightSelectedQuad) {
                        m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], selected_ghost_color, m_annotationData[i + m_preAptReflectionPairs.size()]);
                    }
                    else {
                        glm::vec3 ghost_color = m_light_intensity * postAPTtransmissions[i];
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
                if (m_takeSnapshot != 0) {
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

                    m_takeSnapshot = 0;
                }
             
                if (optimizeWithEA) {
                    //Optimize
                    m_lensSystem = solve_Annotations(m_lensSystem, m_snapshotData, yawandPitch.x, yawandPitch.y);
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();

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

                //Reset atomic counter
                GLuint zero = 0;
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBufferSnapshot);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

            }
            //BUILD FROM SCRATCH
            else {
                //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, build_ssboQuadData);
                //glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 5, build_atomicCounterBuffer);

				m_builderShader.bind();
                //Global uniforms
                glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp));
				glUniform1f(1, irisApertureHeight);
                glActiveTexture(GL_TEXTURE0); // Bind texture
                glBindTexture(GL_TEXTURE_2D, texApt);
                glUniform1i(5, 0);
                glUniform1i(6, cursorPos.x);
                glUniform1i(7, cursorPos.y);
                glUniform1i(9, m_getGhostsAtMouse ? 1 : 0);
                glUniformMatrix4fv(10, 1, GL_FALSE, glm::value_ptr(m_sensorMatrix));
                //Local uniforms and draw ghosts
                glm::vec3 buildGhostColor = glm::vec3(1.0f, 1.0f, 1.0f) * m_light_intensity / 100.f;
                glm::vec3 buildSelectedGhostColor = glm::vec3(10.0f, 0.0f, 0.0f);
				for (int i = 0; i < m_lens_builder_quads.size(); i++) {
                    if (m_selectedQuadIndex != -1 && m_lens_builder_quads[i].getID() == m_selectedQuadIDs[m_selectedQuadIndex]) {
                        m_lens_builder_quads[i].drawQuad(buildSelectedGhostColor, m_annotationData[i]);
                    }
                    else {
                        m_lens_builder_quads[i].drawQuad(buildGhostColor, m_annotationData[i]);
                    }
				}

                if (m_getGhostsAtMouse) {
                    GLuint numQuadsAtPixel = 0;
                    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, build_atomicCounterBuffer);
                    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &numQuadsAtPixel);

                    m_selectedQuadIDs.clear();
                    m_selectedQuadIDs.reserve(numQuadsAtPixel);

                    glBindBuffer(GL_SHADER_STORAGE_BUFFER, build_ssboQuadData);
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
                        
                    }
                    else {
                        m_selectedQuadIndex = -1;
                    }

                    m_getGhostsAtMouse = false;
                }

                if (optimizeWithEA) {
					std::vector<SnapshotData> snapshotData;
					for (auto& AnnotationData : m_annotationData) {
						SnapshotData conversion;
                        conversion.quadHeight = (irisApertureHeight / 2) * AnnotationData.sizeAnnotationTransform;
                        conversion.quadCenterPos = AnnotationData.posAnnotationTransform;
						snapshotData.push_back(conversion);
					}
                    //Optimize
                    m_lensSystem = solve_Annotations(snapshotData, yawandPitch.x, yawandPitch.y);
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();

                    refreshMatricesAndQuads();

                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    irisAperturePosMemory = irisAperturePos;

                    optimizeWithEA = false;
                    m_resetAnnotations = true;
                    m_buildFromScratch = false;
					m_calibrateLightSource = true;
                }

                //Reset atomic counter
                GLuint zero = 0;
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, build_atomicCounterBuffer);
                glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);


            }

            //RENDER LIGHT SOURCE
            m_lightShader.bind();
            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp));
            glUniform3fv(1, 1, glm::value_ptr(m_lcolor));
            glUniform3fv(2, 1, glm::value_ptr(m_light_pos));
            glBindVertexArray(vao_light);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>((lightSphere.triangles.size() * 3)), GL_UNSIGNED_INT, nullptr);

            //RENDER STARBURST
            glm::mat4 starburstMatrix = glm::translate(glm::mat4(1.0f), m_light_pos);
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
        //case GLFW_KEY_W:
        //case GLFW_KEY_A:
        //case GLFW_KEY_S:
        //case GLFW_KEY_D:
        //    m_takeSnapshot = 1;
        //    break;
        default:
            break;
        }
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        //std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
        if (m_window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (m_window.isKeyPressed(GLFW_KEY_Q)) { // Q for moving the selected ghost
                if (m_selectedQuadIndex != -1) {
                    glm::vec4 quadCenterScreenPos;
                    for (SnapshotData& snapshotdata : m_snapshotData) {
                        if (snapshotdata.quadID == m_selectedQuadIDs[m_selectedQuadIndex]) {
                            quadCenterScreenPos = m_mvp * m_sensorMatrix * glm::vec4(snapshotdata.quadCenterPos, 30.0, 1.0);
                            break;
                        }
                    }
                    quadCenterScreenPos = quadCenterScreenPos / quadCenterScreenPos.w;
                    const glm::ivec2& window_size = m_window.getWindowSize();
                    glm::vec2 currentCursorPos = cursorPos;
                    currentCursorPos = (currentCursorPos - glm::vec2(window_size.x / 4.f + (((3.f * window_size.x) / 4.f) / 2.f), window_size.y / 2.f)) / glm::vec2(((3.f * window_size.x) / 4.f) / 2.f, window_size.y / 2.f); //NDC

					glm::vec4 lightPosNDC = m_mvp * glm::vec4(m_light_pos, 1.0);
					lightPosNDC = lightPosNDC / lightPosNDC.w;

                    float lightLineProjectionScalar = glm::dot(currentCursorPos, glm::vec2(lightPosNDC)) / glm::dot(glm::vec2(lightPosNDC), glm::vec2(lightPosNDC));
					glm::vec2 lightLineProjection = lightLineProjectionScalar * glm::vec2(lightPosNDC);

                    // Calculate the diffVectorNDC using the projected cursor position
                    glm::vec2 diffVectorNDC = glm::vec2(lightLineProjection) - glm::vec2(quadCenterScreenPos.x, quadCenterScreenPos.y);
                    glm::vec4 worldSpaceVector = glm::inverse(m_sensorMatrix) * glm::inverse(m_mvp) * glm::vec4(diffVectorNDC, quadCenterScreenPos.z, 1.f);
                    worldSpaceVector = worldSpaceVector / worldSpaceVector.w;
                    int annotationIndex = -1;
                    if (!m_buildFromScratch) {
                        annotationIndex = m_selectedQuadIDs[m_selectedQuadIndex];
                    }
                    else {
						for (int i = 0; i < m_lens_builder_quads.size(); i++) {
							if (m_lens_builder_quads[i].getID() == m_selectedQuadIDs[m_selectedQuadIndex]) {
								annotationIndex = i;
								break;
							}
						}
					}
                    m_annotationData[annotationIndex].posAnnotationTransform = glm::vec2(worldSpaceVector.x, worldSpaceVector.y);
                }
            }
            else if (m_window.isKeyPressed(GLFW_KEY_E) || m_window.isKeyPressed(GLFW_KEY_A) || m_window.isKeyPressed(GLFW_KEY_S) || m_window.isKeyPressed(GLFW_KEY_D)) { // E for scaling the selected ghost
                if (m_selectedQuadIndex != -1) {
					glm::vec2 currentCursorPos = cursorPos; // convert to glm::vec2 for operations
                    glm::vec2 cursorResizeVector = currentCursorPos - m_cursorClickInitialPos;
                    float resizeWeight = (sqrt(pow(cursorResizeVector.x, 2.f) + pow(cursorResizeVector.y, 2.f)) / (float)m_window.getWindowSize().y);
					std::cout << "Resize weight: " << resizeWeight << std::endl;
                    if (currentCursorPos.x < m_cursorClickInitialPos.x) {
						resizeWeight *= -1.f;
                    }

                    if (m_window.isKeyPressed(GLFW_KEY_E)) {
                        float sensitivity = 0.5f;
						resizeWeight *= sensitivity;
                        int annotationIndex = -1;
                        if (!m_buildFromScratch) {
                            annotationIndex = m_selectedQuadIDs[m_selectedQuadIndex];
                        }
                        else {
                            for (int i = 0; i < m_lens_builder_quads.size(); i++) {
                                if (m_lens_builder_quads[i].getID() == m_selectedQuadIDs[m_selectedQuadIndex]) {
                                    annotationIndex = i;
                                    break;
                                }
                            }
                        }
                        if (m_annotationData[annotationIndex].sizeAnnotationTransform + resizeWeight > 0.01) {
                            m_annotationData[annotationIndex].sizeAnnotationTransform += resizeWeight;
                        }
                    }
                    else if (!m_buildFromScratch) {
                        int interfaceToModify;
                        if (m_selectedQuadIDs[m_selectedQuadIndex] < m_preAptReflectionPairs.size()) {
							interfaceToModify = m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]].y; // modify the second interface in reflection pair
                        }
                        else {
                            interfaceToModify = m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()].y;
                        }

                        if (m_window.isKeyPressed(GLFW_KEY_A)) {
                            float sensitivity = 1.f;
                            resizeWeight *= sensitivity;
                            if (m_lens_interfaces[interfaceToModify].di + resizeWeight > 0.1 && m_lens_interfaces[interfaceToModify].di + resizeWeight < 250) {
                                m_lens_interfaces[interfaceToModify].di += resizeWeight;
                            } 
						}
						else if (m_window.isKeyPressed(GLFW_KEY_S)) {
                            float sensitivity = 0.1f;
                            resizeWeight *= sensitivity;
                            if (m_lens_interfaces[interfaceToModify].ni + resizeWeight > 1 && m_lens_interfaces[interfaceToModify].ni + resizeWeight < 2.5) {
                                m_lens_interfaces[interfaceToModify].ni += resizeWeight;
                            }
							
						}
						else if (m_window.isKeyPressed(GLFW_KEY_D)) {
                            float sensitivity = 10.0f;
                            resizeWeight *= sensitivity;
                            m_lens_interfaces[interfaceToModify].Ri += resizeWeight;
						}

                        m_lensSystem.setLensInterfaces(m_lens_interfaces);
                        refreshMatricesAndQuads();

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
            if (m_window.isKeyPressed(GLFW_KEY_E) || m_window.isKeyPressed(GLFW_KEY_A) || m_window.isKeyPressed(GLFW_KEY_D) || m_window.isKeyPressed(GLFW_KEY_S)) {
                m_cursorClickInitialPos = m_window.getCursorPos();
                //std::cout << "INITIAL POINT AT:" << m_resizeInitialPos.x << ", " << m_resizeInitialPos.y << std::endl;
            }
            else {
                m_getGhostsAtMouse = true;
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
            m_takeSnapshot = 1;
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
    LensSystem m_lensSystem = heliarTronerLens();
    std::vector<LensInterface> m_lens_interfaces;
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
    int m_takeSnapshot = 1;
    std::vector<SnapshotData> m_snapshotData;
    std::vector<glm::vec2> m_quadcenter_points;
    glm::vec2 m_cursorClickInitialPos;

    /* Lens Builder */
    bool m_buildFromScratch = false;
    std::vector<FlareQuad> m_lens_builder_quads;
	int m_buildQuadIDCounter = 0;

    /* Shaders */
    Shader m_defaultShader;
    Shader m_lightShader;
    Shader m_starburstShader;
    Shader m_quadCenterShader;
    Shader m_builderShader;

    /* Light Source */
    const glm::vec3 m_lcolor{ 1, 1, 0.5 };
    /* Light */
    glm::vec3 m_light_pos = { 0.0001f, 0.0001f, 25.f };
    float m_light_intensity = 150.0f;
    bool m_calibrateLightSource = true;

};

int main()
{
    Application app;
    app.update();

    return 0;
}

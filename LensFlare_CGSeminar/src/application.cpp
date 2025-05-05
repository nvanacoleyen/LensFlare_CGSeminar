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
#include <implot/implot.h>
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
#include "coating_solver.h"
#include "aperture_maker.h"

/* GLOBAL PARAMS */
HWND hwnd = GetConsoleWindow();
UINT dpi = GetDpiForWindow(hwnd);
const float DISPLAY_SCALING_FACTOR = static_cast<float>(dpi) / 96.0f;
const char* APERTURE_TEXTURE = "resources/iris.png";

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

        refreshTransmissions(glm::vec2(0.001f), m_quarterWaveCoating);

        m_resetAnnotations = true;
    }

    void refreshTransmissions(glm::vec2 yawandPitch, bool quarterWaveCoating) {
        glm::vec2 post_apt_center_ray_x = glm::vec2(-yawandPitch.x * m_default_Ma[1][0] / m_default_Ma[0][0], yawandPitch.x);
        glm::vec2 post_apt_center_ray_y = glm::vec2(-yawandPitch.y * m_default_Ma[1][0] / m_default_Ma[0][0], yawandPitch.y);
        std::vector<glm::vec2> pre_apt_center_ray_x;
        std::vector<glm::vec2> pre_apt_center_ray_y;
        for (auto& const preAptMa : m_preAptMas) {
            pre_apt_center_ray_x.push_back(glm::vec2(-yawandPitch.x * preAptMa[1][0] / preAptMa[0][0], yawandPitch.x));
            pre_apt_center_ray_y.push_back(glm::vec2(-yawandPitch.y * preAptMa[1][0] / preAptMa[0][0], yawandPitch.y));
        }
        m_preAPTtransmissions = m_lensSystem.getTransmission(m_preAptReflectionPairs, pre_apt_center_ray_x, pre_apt_center_ray_y, quarterWaveCoating);
        m_postAPTtransmissions = m_lensSystem.getTransmission(m_postAptReflectionPairs, post_apt_center_ray_x, post_apt_center_ray_y, quarterWaveCoating);
    }

	void updateStarburstTexture(GLuint texStarburst) {
        createStarburst(APERTURE_TEXTURE);
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("resources/starburst_texture.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            std::cerr << "Failed to load starburst texture" << std::endl;
        }
        glTextureSubImage2D(texStarburst, 0, 0, 0, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
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
        float fov = 50;
        float fovMemory = 50;

        int interfaceToUpdate = 0;
        int interfaceToUpdatePreviousValue = -1;
        float newdi = 10.0f;
        float newni = 1.0f;
        float newRi = 100.0f;
        float newlambda0 = 400.0f;
        float newc_di = 70.f;
        float newc_ni = 1.38f;

        int interfaceToRemove = 0;

        glm::vec3 selected_ghost_color = glm::vec3(1.0f, 0.0f, 0.0f);

        bool highlightSelectedQuad = true;
        int selectedQuadId = -1;
        int selectedQuadIdMemory = -1;
        glm::vec2 selectedQuadReflectionInterfaces;
        int selectedInterfaceInPair = 0;

        bool optimizeCoatings = false;
        bool lensInterfaceRefresh = false;
        bool optimizeLensSystemWithEA = false;
        bool disableEntranceClipping = false;
        bool optimizeLensCoatingsWithEA = false;

        float ghostIntensity = 1.0f;

        bool greyScaleColor = false;

        float starburstScale = 5;

        bool aptMakerOpen = false;
        bool aptMakerClosed = true;
        updateAptImage();
        updateAptTexture(texApt);

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

            if (!m_buildFromScratch) {
                //Button loading lens system
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Preset Lens Systems");
                if (ImGui::Button("Load Heliar Troner Lens System")) {
                    m_lensSystem = heliarTronerLens();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_calibrateLightSource = true;
                    m_selectedQuadIndex = -1;
                }
                if (ImGui::Button("Load Canon Lens System")) {
                    m_lensSystem = someCanonLens();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_calibrateLightSource = true;
                    m_selectedQuadIndex = -1;
                }
                if (ImGui::Button("Load Test Lens System")) {
                    m_lensSystem = japanesePatent();
                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_calibrateLightSource = true;
                    m_selectedQuadIndex = -1;
                }
            }

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Global Lens Settings");
            
            ImGui::SliderFloat("Lens FOV", &fov, 10.0f, 120.0f);
            if (fov != fovMemory) {
                m_mainProjectionMatrix = glm::perspective(glm::radians(fov), m_aspect, 0.01f, 100.0f);
                fovMemory = fov;
            }

            ImGui::SliderFloat("Starburst Scale", &starburstScale, 0.0f, 40.0f);

            if (ImGui::Button("Modify Aperture")) {
                aptMakerOpen = true;
                ImGui::OpenPopup("Aperture Generator");
            }

            if (aptMakerOpen) {
                createApt(&aptMakerOpen, texApt);
            }

            if (aptMakerClosed && !aptMakerOpen)
            {
                updateStarburstTexture(texStarburst);
            }
            aptMakerClosed = aptMakerOpen;

            ImGui::SliderFloat("Aperture Height", &m_lensSystem.m_aperture_height, 0.0f, 30.0f);
            if (!m_buildFromScratch) {
                ImGui::InputInt("Aperture Position", &irisAperturePos);
                ImGui::SliderFloat("Entrance Pupil Height", &m_lensSystem.m_entrance_pupil_height, 0.0f, 50.0f);
                ImGui::Checkbox("Disable Entrance Clipping", &disableEntranceClipping);
                ImGui::Checkbox("Grey Scale", &greyScaleColor);
                ImGui::Text("Coatings: ");
                if (ImGui::RadioButton("Quarter Wave", &m_quarterWaveCoating, 1)) {
                    refreshTransmissions(m_yawandPitch, m_quarterWaveCoating);
                    m_calibrateLightSource = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Custom", &m_quarterWaveCoating, 0)) {
                    refreshTransmissions(m_yawandPitch, m_quarterWaveCoating);
                    m_calibrateLightSource = true;
                }

                if (!(m_optimizeInterfacesWithEA || m_optimizeCoatingsWithEA)) {

                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lens Interface Settings");
                    if (ImGui::CollapsingHeader("Lens Prescription")) {
                        if (m_quarterWaveCoating) {
                            ImGui::BeginTable("Lens Prescription", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                        }
                        else {
                            ImGui::BeginTable("Lens Prescription", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                        }
                        ImGui::TableSetupColumn("Index");
                        ImGui::TableSetupColumn("d");
                        ImGui::TableSetupColumn("n");
                        ImGui::TableSetupColumn("R");
                        if (m_quarterWaveCoating) {
                            ImGui::TableSetupColumn("lambda0");
                        }
                        else {
                            ImGui::TableSetupColumn("c_d");
                            ImGui::TableSetupColumn("c_n");
                        }
                        ImGui::TableHeadersRow();

                        for (int i = 0; i < m_lens_interfaces.size(); i++) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%d", i);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", m_lens_interfaces[i].di);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%.3f", m_lens_interfaces[i].ni);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%.3f", m_lens_interfaces[i].Ri);
                            ImGui::TableSetColumnIndex(4);
                            if (m_quarterWaveCoating) {
                                ImGui::Text("%.3f", m_lens_interfaces[i].lambda0);
                            }
                            else {
                                ImGui::Text("%.3f", m_lens_interfaces[i].c_di);
                                ImGui::TableSetColumnIndex(5);
                                ImGui::Text("%.3f", m_lens_interfaces[i].c_ni);
                            }
                        }

                        ImGui::EndTable();
                    }


                    if (ImGui::CollapsingHeader("Modify Interface")) {
                        ImGui::InputInt("Interface to Update", &interfaceToUpdate);
                        int interfaceCount = m_lens_interfaces.size();

                        // If index is valid show sliders
                        if (interfaceToUpdate >= 0 && interfaceToUpdate < interfaceCount) {
                            LensInterface& lensInterface = m_lens_interfaces[interfaceToUpdate];

                            if (ImGui::SliderFloat("Thickness", &lensInterface.di, 0.001f, 250.0f)) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                                lensInterfaceRefresh = true;
                            }
                            if (ImGui::SliderFloat("Refractive Index", &lensInterface.ni, 1.0f, 2.5f)) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                                lensInterfaceRefresh = true;
                            }
                            int convexLens = 0;
                            if (lensInterface.Ri > 0) {
							    convexLens = 1;
						    }
                            bool radioButtonChange = false;
                            if (ImGui::RadioButton("Convex", &convexLens, 1)) {
                                radioButtonChange = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::RadioButton("Concave", &convexLens, 0)) {
                                radioButtonChange = true;
                            }
                            float minRadius = 1.0f;
                            float maxRadius = 1000.0f;
                            float sliderValue = (log(abs(lensInterface.Ri)) - log(minRadius)) / (log(maxRadius) - log(minRadius));
                            ImGui::SliderFloat("Radius", &sliderValue, 0.0f, 1.0f, std::format("{:.3f}", lensInterface.Ri).c_str());
                            lensInterface.Ri = exp(log(minRadius) + sliderValue * (log(maxRadius) - log(minRadius)));
                            if (convexLens == 0) {
							    lensInterface.Ri = -lensInterface.Ri;
                            }
                            if (ImGui::IsItemEdited() || radioButtonChange) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                                lensInterfaceRefresh = true;
                            }
                            if (m_quarterWaveCoating) {
                                if (ImGui::SliderFloat("Lambda0", &lensInterface.lambda0, 380.0f, 740.0f)) {
                                    m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                    refreshMatricesAndQuads();
                                    lensInterfaceRefresh = true;
                                }
                            }
                            else {
                                if (ImGui::SliderFloat("Coating Thickness", &lensInterface.c_di, 25, 750.0f)) {
                                    m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                    refreshMatricesAndQuads();
                                }
                                if (ImGui::SliderFloat("Coating Refractive Index", &lensInterface.c_ni, 1.38f, 1.9f)) {
                                    m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                    refreshMatricesAndQuads();
                                }
                            }
                            
                        }
                        // If index equals the size of the vector, allow adding a new interface
                        else if (interfaceToUpdate == interfaceCount) {
                            ImGui::Text("End of System. Create new Interface:");
                            ImGui::SliderFloat("Thickness", &newdi, 0.001f, 250.0f);
                            ImGui::SliderFloat("Refractive Index", &newni, 1.0f, 2.5f);
                            int convexLens = 0;
                            if (newRi > 0) {
                                convexLens = 1;
                            }

                            ImGui::RadioButton("Convex", &convexLens, 1);
                            ImGui::SameLine();
                            ImGui::RadioButton("Concave", &convexLens, 0);
                            float minRadius = 1.0f;
                            float maxRadius = 1000.0f;
                            float sliderValue = (log(abs(newRi)) - log(minRadius)) / (log(maxRadius) - log(minRadius));
                            ImGui::SliderFloat("Radius", &sliderValue, 0.0f, 1.0f, std::format("{:.3f}", newRi).c_str());
                            newRi = exp(log(minRadius) + sliderValue * (log(maxRadius) - log(minRadius)));
                            if (convexLens == 0) {
                                newRi = -newRi;
                            }
                            if (m_quarterWaveCoating) {
                                ImGui::SliderFloat("Lambda0", &newlambda0, 380.0f, 740.0f);
                            }
                            else {
                                ImGui::SliderFloat("Coating Thickness", &newc_di, 25, 750);
                                ImGui::SliderFloat("Coating Refractive Index", &newc_ni, 1.38f, 1.9f);
                            }

                            if (ImGui::Button("Add Interface")) {
                                LensInterface newLensInterface(newdi, newni, newRi, newlambda0);
                                m_lens_interfaces.push_back(newLensInterface);
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                                newdi = 10.0f;
                                newni = 1.0f;
                                newRi = 100.0f;
                                newlambda0 = 400.0f;
                                newc_di = 50.0f;
                                newc_ni = 1.38f;
								lensInterfaceRefresh = true;
                            }
                        }
                        else {
                            ImGui::Text("Invalid index.");
                        }
                    }


                    if (ImGui::CollapsingHeader("Remove Interface")) {
                        ImGui::InputInt("Interface to Remove", &interfaceToRemove);
                        if (ImGui::Button("Remove")) {
                            if (interfaceToRemove >= 0 && interfaceToRemove < m_lens_interfaces.size()) {
                                m_lens_interfaces.erase(m_lens_interfaces.begin() + interfaceToRemove);
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
								lensInterfaceRefresh = true;
                                m_selectedQuadIndex = -1;
                            }
                        }
                    }

                    if (m_selectedQuadIndex != -1) {

                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Ghost Settings");
                        ImGui::ColorEdit3("Highlight Color", (float*)&selected_ghost_color);
                        ImGui::Checkbox("Highlight Quad", &highlightSelectedQuad);
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
                            selectedQuadIdMemory = selectedQuadId;
                            lensInterfaceRefresh = false;
                            auto reflectivityData = computeReflectivityPerLambda(m_lensSystem, selectedQuadReflectionInterfaces, m_yawandPitch);
                            m_reflectivity_per_lambda_first_interface = reflectivityData.first;
                            m_reflectivity_per_lambda_second_interface = reflectivityData.second;
                        }

                        if (m_quarterWaveCoating) {

                            std::vector<float> lambda_values, reflectivityR, reflectivityG, reflectivityB;
                            if (selectedInterfaceInPair == 0) {
                                for (const auto& entry : m_reflectivity_per_lambda_first_interface) {
                                    lambda_values.push_back(entry.first);  // Lambda
                                    reflectivityR.push_back(entry.second.r); // Red channel reflectivity
                                    reflectivityG.push_back(entry.second.g); // Green channel reflectivity
                                    reflectivityB.push_back(entry.second.b); // Blue channel reflectivity
                                }
                            }
                            else {
                                for (const auto& entry : m_reflectivity_per_lambda_second_interface) {
                                    lambda_values.push_back(entry.first);  // Lambda
                                    reflectivityR.push_back(entry.second.r); // Red channel reflectivity
                                    reflectivityG.push_back(entry.second.g); // Green channel reflectivity
                                    reflectivityB.push_back(entry.second.b); // Blue channel reflectivity
                                }
                            }
                            

                            if (ImPlot::BeginPlot("Reflectivity vs Wavelength")) {
                                ImPlot::SetupAxes("Wavelength (nm)", "Reflectivity", ImPlotAxisFlags_AutoFit);

                                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); //r
                                ImPlot::PlotLine("Red Channel", lambda_values.data(), reflectivityR.data(), lambda_values.size());
                                ImPlot::PopStyleColor();

                                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); //g
                                ImPlot::PlotLine("Green Channel", lambda_values.data(), reflectivityG.data(), lambda_values.size());
                                ImPlot::PopStyleColor();

                                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 0.0f, 1.0f, 1.0f)); //b
                                ImPlot::PlotLine("Blue Channel", lambda_values.data(), reflectivityB.data(), lambda_values.size());
                                ImPlot::PopStyleColor();

                                ImPlot::EndPlot();
                            }

                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "Selected Ghost Color:");
							ImGui::SameLine();
                            glm::vec3 colorOfSelectedGhost;
                            if (selectedQuadId < m_preAPTtransmissions.size()) {
                                colorOfSelectedGhost = m_preAPTtransmissions[selectedQuadId];
                            }
                            else {
                                colorOfSelectedGhost = m_postAPTtransmissions[selectedQuadId - m_preAPTtransmissions.size()];
                            }
                            colorOfSelectedGhost *= m_light_intensity;
                            colorOfSelectedGhost = normalizeRGB(colorOfSelectedGhost);
                            ImGui::ColorButton("Selected Ghost Color", ImVec4(colorOfSelectedGhost.x, colorOfSelectedGhost.y, colorOfSelectedGhost.z, 0.5f));

                            if (ImGui::Button("Optimize Ghost Color To Highlight Color")) {
                                optimizeCoatings = true;
                                highlightSelectedQuad = false;
                            }
                        }


                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.0f, 1.0f), "Reflection Interface Settings:");

                        ImGui::RadioButton("First Reflection Interface", &selectedInterfaceInPair, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Second Reflection Interface", &selectedInterfaceInPair, 1);

                        LensInterface& lensInterface = m_lens_interfaces[selectedQuadReflectionInterfaces[selectedInterfaceInPair]];

                        if (ImGui::SliderFloat("Thickness ", &lensInterface.di, 0.001f, 200.0f)) {
                            m_lensSystem.setLensInterfaces(m_lens_interfaces);
                            refreshMatricesAndQuads();
							lensInterfaceRefresh = true;
                        }
                        if (ImGui::SliderFloat("Refractive Index ", &lensInterface.ni, 1.0f, 2.5f)) {
                            m_lensSystem.setLensInterfaces(m_lens_interfaces);
                            refreshMatricesAndQuads();
							lensInterfaceRefresh = true;
                        }
                        int convexLens = 0;
                        if (lensInterface.Ri > 0) {
                            convexLens = 1;
                        }
                        bool radioButtonChange = false;
                        if (ImGui::RadioButton("Convex ", &convexLens, 1)) {
                            radioButtonChange = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::RadioButton("Concave ", &convexLens, 0)) {
                            radioButtonChange = true;
                        }
                        float minRadius = 1.0f;
                        float maxRadius = 1000.0f;
                        float sliderValue = (log(abs(lensInterface.Ri)) - log(minRadius)) / (log(maxRadius) - log(minRadius));
                        ImGui::SliderFloat("Radius ", &sliderValue, 0.0f, 1.0f, std::format("{:.3f}", lensInterface.Ri).c_str());
                        lensInterface.Ri = exp(log(minRadius) + sliderValue * (log(maxRadius) - log(minRadius)));
                        if (convexLens == 0) {
                            lensInterface.Ri = -lensInterface.Ri;
                        }
                        if (ImGui::IsItemEdited() || radioButtonChange) {
                            m_lensSystem.setLensInterfaces(m_lens_interfaces);
                            refreshMatricesAndQuads();
                            lensInterfaceRefresh = true;
                        }

                        if (m_quarterWaveCoating) {
                            if (ImGui::SliderFloat("Lambda0 ", &lensInterface.lambda0, 380.0f, 740.0f)) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                                lensInterfaceRefresh = true;
                            }
                        }
                        else {
                            if (ImGui::SliderFloat("Coating Thickness ", &lensInterface.c_di, 25, 750.0f)) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                            }
                            if (ImGui::SliderFloat("Coating Refractive Index ", &lensInterface.c_ni, 1.38f, 1.9f)) {
                                m_lensSystem.setLensInterfaces(m_lens_interfaces);
                                refreshMatricesAndQuads();
                            }
                        }

                        if (m_quarterWaveCoating) {
                            ImGui::BeginTable("Lens Prescription", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                        }
                        else {
                            ImGui::BeginTable("Lens Prescription", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg);
                        }

                        ImGui::TableSetupColumn("Index");
                        ImGui::TableSetupColumn("d");
                        ImGui::TableSetupColumn("n");
                        ImGui::TableSetupColumn("R");
                        if (m_quarterWaveCoating) {
                            ImGui::TableSetupColumn("lambda0");
                        }
                        else {
                            ImGui::TableSetupColumn("c_d");
                            ImGui::TableSetupColumn("c_n");
                        }
                        ImGui::TableHeadersRow();

                        for (int i = 0; i < 2; ++i) {
                            int interfaceIndex = selectedQuadReflectionInterfaces[i];
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text("%d", interfaceIndex);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].di);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].ni);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].Ri);
                            ImGui::TableSetColumnIndex(4);
                            if (m_quarterWaveCoating) {
                                ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].lambda0);
                            }
                            else {
                                ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].c_di);
                                ImGui::TableSetColumnIndex(5);
                                ImGui::Text("%.3f", m_lens_interfaces[interfaceIndex].c_ni);
                            }
                        }

                        ImGui::EndTable();

                        

                    }
                    else {
                        selectedQuadId = -1;
                        selectedQuadIdMemory = -1;
                    }

                }
            
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lens Optimizations");

				if (m_optimizeInterfacesWithEA) {
                    if (ImGui::Button("Abort")) {
						m_optimizeInterfacesWithEA = false;
                        m_resetAnnotations = true;
                    }
                    if (ImGui::Button("Reset Annotations")) {
                        m_resetAnnotations = true;
                    }
                    ImGui::SliderFloat("Ghost Intensity", &ghostIntensity, 0.1, 2);
                    if (ImGui::Button("Run EA")) {
                        m_takeSnapshot = 2;
                        optimizeLensSystemWithEA = true;
                        m_optimizeInterfacesWithEA = false;
                    }
				}
                else if (m_optimizeCoatingsWithEA) {
                    if (ImGui::Button("Abort")) {
                        m_optimizeCoatingsWithEA = false;
                        m_resetAnnotations = true;
                    }
                    if (ImGui::Button("Reset Annotations")) {
                        m_resetAnnotations = true;
                    }
                    if (m_selectedQuadIndex != 1) {
                        ImGui::ColorEdit3("Ghost Color", (float*)&selected_ghost_color);
                        ImGui::Checkbox("Highlight Quad", &highlightSelectedQuad);
                    }
                    if (ImGui::Button("Annotate Color")) {
                        if (m_selectedQuadIndex != -1) {
                            //get pairs of the id reflection
                            selectedQuadId = m_selectedQuadIDs[m_selectedQuadIndex];
                            m_colorAnnotations[selectedQuadId] = selected_ghost_color;
                        }
                    }
                    if (ImGui::Button("Run EA")) {
                        optimizeLensCoatingsWithEA = true;
                        m_optimizeCoatingsWithEA = false;
                    }
                }
				else {
					if (ImGui::Button("Optimize Ghost Size and Location")) {
						selected_ghost_color = glm::vec3(1.0f, 0.0f, 0.0f);
						m_optimizeInterfacesWithEA = true;
						m_resetAnnotations = true;
						highlightSelectedQuad = true;
					}
					if (ImGui::Button("Optimize Colors")) {
						m_optimizeCoatingsWithEA = true;
						m_resetAnnotations = true;
					}

                }
            }

            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Lens Creator");
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
				if (ImGui::Button("Build")) {
                    //RUN EA and reset params
                    optimizeLensSystemWithEA = true;
				}

            }
            else {
                if (ImGui::Button("Build from Scratch")) {
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
                m_selectedQuadIndex = -1;
            }

            // Reset annotations
            if (m_resetAnnotations) {
                m_annotationData.clear();
                m_colorAnnotations.clear();
				int amount_ghosts = 0;
                if (!m_buildFromScratch) {
                    amount_ghosts = m_preAptReflectionPairs.size() + m_postAptReflectionPairs.size();
                }
                else {
                    amount_ghosts = m_lens_builder_quads.size();
                }
                for (int i = 0; i < amount_ghosts; i++) {
                    m_annotationData.push_back(AnnotationData());
					m_colorAnnotations.push_back(glm::vec3(-1.0f, -1.0f, -1.0f));
                }
                m_resetAnnotations = false;
            }

            glm::vec2 cursorPos = m_window.getCursorPos();

            // Update Projection Matrix
            m_mvp = m_mainProjectionMatrix * m_camera.viewMatrix();
            const glm::vec3 cameraPos = m_camera.cameraPos();
            const glm::vec3 cameraForward = m_camera.m_forward;
            const glm::vec3 cameraUp = m_camera.m_up;
            m_yawandPitch = getYawandPitch(cameraPos, cameraForward, cameraUp, m_light_pos);
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

                if (optimizeCoatings && m_selectedQuadIndex != -1 && m_quarterWaveCoating) {
                    if (m_selectedQuadIDs[m_selectedQuadIndex] < m_preAptReflectionPairs.size()) {
                        optimizeLensCoatingsGridSearch(m_lensSystem, selected_ghost_color, m_preAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex]], glm::vec2(0.001));
                    }
                    else {
                        optimizeLensCoatingsGridSearch(m_lensSystem, selected_ghost_color, m_postAptReflectionPairs[m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size()], glm::vec2(0.001));
                    }
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshTransmissions(m_yawandPitch, m_quarterWaveCoating);
                    lensInterfaceRefresh = true;
                    optimizeCoatings = false;
                    m_calibrateLightSource = true;
                }

                if (m_calibrateLightSource) {
                    double totalSum = 0.0;
                    for (const auto& vec : m_preAPTtransmissions) {
                        totalSum += vec.x + vec.y + vec.z;
                    }
                    for (const auto& vec : m_postAPTtransmissions) {
                        totalSum += vec.x + vec.y + vec.z;
                    }
					m_light_intensity = (1.0f / (totalSum)) * 3;
					min_light_intensity = 0.1f;
					max_light_intensity = m_light_intensity * 7.0f;

                    if (std::isinf(max_light_intensity)) {
                        max_light_intensity = 1000000.0f;
                    }

					m_calibrateLightSource = false;
                }

                //RENDERING
                m_defaultShader.bind();

                /* Bind General Variables */
                glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(m_mvp)); // Projection Matrix
                glUniform1f(4, m_yawandPitch.x);
                glUniform1f(5, m_yawandPitch.y);
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
                    else if (m_optimizeInterfacesWithEA || greyScaleColor) {
                        glm::vec3 greyscaleColor = glm::vec3((1.f / m_annotationData.size()) * 2 * ghostIntensity);
                        m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, greyscaleColor, m_annotationData[i]);
                    }
                    else {
                        if (m_optimizeCoatingsWithEA && m_colorAnnotations[i] != glm::vec3(-1.0f, -1.0f, -1.0f)) {
							m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, m_colorAnnotations[i], m_annotationData[i]);
						}
                        else {
                            glm::vec3 ghost_color = m_light_intensity * m_preAPTtransmissions[i];
                            m_preAptQuads[i].drawQuad(m_preAptMas[i], m_default_Ms, ghost_color, m_annotationData[i]);
                        }
                    }
                }
                for (int i = 0; i < m_postAptReflectionPairs.size(); i++) {
                    if (m_selectedQuadIndex != -1 && m_selectedQuadIDs[m_selectedQuadIndex] - m_preAptReflectionPairs.size() == i && highlightSelectedQuad) {
                        m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], selected_ghost_color, m_annotationData[i + m_preAptReflectionPairs.size()]);
                    }
                    else if (m_optimizeInterfacesWithEA || greyScaleColor) {
                        glm::vec3 greyscaleColor = glm::vec3((1.f / m_annotationData.size()) * 2 * ghostIntensity);
                        m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], greyscaleColor, m_annotationData[i + m_preAptReflectionPairs.size()]);
                    }
                    else {
                        if (m_optimizeCoatingsWithEA && m_colorAnnotations[i + m_preAptReflectionPairs.size()] != glm::vec3(-1.0f, -1.0f, -1.0f)) {
                            m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], m_colorAnnotations[i + m_preAptReflectionPairs.size()], m_annotationData[i + m_preAptReflectionPairs.size()]);
                        }
                        else {
                            glm::vec3 ghost_color = m_light_intensity * m_postAPTtransmissions[i];
                            m_postAptQuads[i].drawQuad(m_default_Ma, m_postAptMss[i], ghost_color, m_annotationData[i + m_preAptReflectionPairs.size()]);
                        }
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
             
                if (optimizeLensSystemWithEA) {
                    //Optimize
                    eaTop5Systems = solveLensAnnotations(m_lensSystem, m_snapshotData, m_yawandPitch.x, m_yawandPitch.y);
                    eaTop5SystemsIndex = 0;
					m_lensSystem = eaTop5Systems[0];
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();

                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;

                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    irisAperturePosMemory = irisAperturePos;

                    optimizeLensSystemWithEA = false;
                    m_resetAnnotations = true;
                    m_calibrateLightSource = true;
                }

                if (optimizeLensCoatingsWithEA) {
                    std::vector<glm::vec3> renderObjective;
                    for (int i = 0; i < m_colorAnnotations.size(); i++) {
                        if (m_colorAnnotations[i] != glm::vec3(-1.0f, -1.0f, -1.0f)) {
                            renderObjective.push_back(m_colorAnnotations[i]);
                        }
                        else {
							if (i < m_preAptReflectionPairs.size()) {
								renderObjective.push_back(m_preAPTtransmissions[i] * m_light_intensity);
							}
							else {
								renderObjective.push_back(m_postAPTtransmissions[i - m_preAptReflectionPairs.size()] * m_light_intensity);
							}
                        }
                    }

                    m_lensSystem = solveCoatingAnnotations(m_lensSystem, renderObjective, m_yawandPitch.x, m_yawandPitch.y, m_light_intensity, m_quarterWaveCoating);
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();
                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;
					optimizeLensCoatingsWithEA = false;
					m_resetAnnotations = true;
                    m_calibrateLightSource = true;
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

                    if (!m_selectedQuadIDs.empty()) {
                        m_selectedQuadIndex = 0;
                        std::cout << "Selected Ghost: " << m_selectedQuadIDs[m_selectedQuadIndex] << std::endl;
                        
                    }
                    else {
                        m_selectedQuadIndex = -1;
                    }

                    m_getGhostsAtMouse = false;
                }

                if (optimizeLensSystemWithEA) {
					std::vector<SnapshotData> snapshotData;
					for (auto& AnnotationData : m_annotationData) {
						SnapshotData conversion;
                        conversion.quadHeight = (irisApertureHeight / 2) * AnnotationData.sizeAnnotationTransform;
                        conversion.quadCenterPos = AnnotationData.posAnnotationTransform;
						snapshotData.push_back(conversion);
					}
                    //Optimize
                    eaTop5Systems = solveLensAnnotations(snapshotData, m_yawandPitch.x, m_yawandPitch.y);
                    eaTop5SystemsIndex = 0;
					m_lensSystem = eaTop5Systems[0];
                    m_lens_interfaces = m_lensSystem.getLensInterfaces();

                    refreshMatricesAndQuads();
                    m_selectedQuadIndex = -1;

                    irisAperturePos = m_lensSystem.getIrisAperturePos();
                    irisAperturePosMemory = irisAperturePos;
                    m_lensSystem.setEntrancePupilHeight(40);

                    optimizeLensSystemWithEA = false;
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
        case GLFW_KEY_RIGHT:
            if (eaTop5Systems.size() > 0) {
				eaTop5SystemsIndex++;
				if (eaTop5SystemsIndex >= eaTop5Systems.size()) {
					eaTop5SystemsIndex = 0;
				}

                m_lensSystem = eaTop5Systems[eaTop5SystemsIndex];
                m_lens_interfaces = m_lensSystem.getLensInterfaces();

                refreshMatricesAndQuads();
                m_selectedQuadIndex = -1;

                irisAperturePos = m_lensSystem.getIrisAperturePos();
                irisAperturePosMemory = irisAperturePos;

                m_resetAnnotations = true;
                m_calibrateLightSource = true;

            }
            break;
        case GLFW_KEY_R:
            m_camera.m_forward = glm::normalize(-glm::vec3(0.0001f, 0.0001f, -1.0f));
			m_camera.m_up = glm::vec3(0.0f, 1.0f, 0.0f);
            m_takeSnapshot = 1;
            break;
        case GLFW_KEY_S:
            m_window.renderToImage("C:/Users/neilv/Desktop/Results/Renders/flare_render" + std::to_string(renderSaveCount) + ".png", true);
            renderSaveCount++;
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
		if (m_optimizeInterfacesWithEA || m_buildFromScratch) {
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
                else if (m_window.isKeyPressed(GLFW_KEY_E)) { // E for scaling the selected ghost
                    if (m_selectedQuadIndex != -1) {
					    glm::vec2 currentCursorPos = cursorPos; // convert to glm::vec2 for operations
                        glm::vec2 cursorResizeVector = currentCursorPos - m_cursorClickInitialPos;
                        float resizeWeight = (sqrt(pow(cursorResizeVector.x, 2.f) + pow(cursorResizeVector.y, 2.f)) / (float)m_window.getWindowSize().y);
					    std::cout << "Resize weight: " << resizeWeight << std::endl;
                        if (currentCursorPos.x < m_cursorClickInitialPos.x) {
						    resizeWeight *= -1.f;
                        }

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
            if (m_window.isKeyPressed(GLFW_KEY_E) && (m_optimizeInterfacesWithEA || m_buildFromScratch)) {
                m_cursorClickInitialPos = m_window.getCursorPos();
                //std::cout << "INITIAL POINT AT:" << m_resizeInitialPos.x << ", " << m_resizeInitialPos.y << std::endl;
            }
            else if (m_window.isKeyPressed(GLFW_KEY_Q)) {
				//do nothing
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
    Camera m_camera{ &m_window, glm::vec3(0.0f, 0.0f, -1.0f), -glm::vec3(0.0001f, 0.0001f, -1.0f), 0 };
    float m_fov;
    float m_distance;
    float m_aspect;
    glm::mat4 m_mainProjectionMatrix;
    glm::mat4 m_mvp;
    glm::vec2 m_yawandPitch = - m_camera.getYawAndPitch();

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
    std::vector<glm::vec3> m_preAPTtransmissions;
    std::vector<glm::vec3> m_postAPTtransmissions;
    int m_quarterWaveCoating = true;
    std::vector<std::pair<float, glm::vec3>> m_reflectivity_per_lambda_first_interface;
    std::vector<std::pair<float, glm::vec3>> m_reflectivity_per_lambda_second_interface;

    std::vector<LensSystem> eaTop5Systems;
    int eaTop5SystemsIndex = 0;

    int irisAperturePos = m_lensSystem.getIrisAperturePos();
    int irisAperturePosMemory = irisAperturePos;

    glm::mat4 m_sensorMatrix;

    /* Ghost Selection */
    bool m_getGhostsAtMouse = false;
    std::vector<int> m_selectedQuadIDs;
    int m_selectedQuadIndex = -1;

    /* Ghost Annotations */
    bool m_resetAnnotations = true;
    std::vector<AnnotationData> m_annotationData;
    int m_takeSnapshot = 1;
    std::vector<SnapshotData> m_snapshotData;
    std::vector<glm::vec2> m_quadcenter_points;
    glm::vec2 m_cursorClickInitialPos;

    /* Coating Annotations */
	std::vector<glm::vec3> m_colorAnnotations;

    /* Lens Builder */
    bool m_buildFromScratch = false;
    bool m_optimizeInterfacesWithEA = false;
    bool m_optimizeCoatingsWithEA = false;
    std::vector<FlareQuad> m_lens_builder_quads;
	int m_buildQuadIDCounter = 0;

    /* Shaders */
    Shader m_defaultShader;
    Shader m_lightShader;
    Shader m_starburstShader;
    Shader m_quadCenterShader;
    Shader m_builderShader;

    /* Light Source */
    const glm::vec3 m_lcolor{ 1, 1, 0.7 };
    /* Light */
    glm::vec3 m_light_pos = { 0.0001f, 0.0001f, 20.f };
    float m_light_intensity = 150.0f;
    bool m_calibrateLightSource = true;

    int renderSaveCount = 1;

};

int main()
{
    Application app;
    app.update();

    return 0;
}

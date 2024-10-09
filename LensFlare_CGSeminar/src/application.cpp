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
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <cmath>
#include "ray_transfer_matrices.h"
#include "line_drawer.h"
#include "lens_system.h"
#include <limits>

//UTIL FUNCTIONS
float toRad(float degrees) {
    return degrees * 3.141593f / 180.0f;
}

LineDrawer raytoLine(float z, glm::vec2 ray) {
    std::vector<glm::vec3> rayLine;
    rayLine.push_back(glm::vec3(z, ray.x, 0.0f));
    rayLine.push_back(glm::vec3(z + cos(ray.y), ray.x + sin(ray.y), 0.0f));

    return LineDrawer(rayLine);
}

LensSystem generateExampleLens() {

    std::vector<LensInterface> lensInterfaces;

    lensInterfaces.push_back(LensInterface(7.7f, 1.652f, 30.81f, 0.0f)); //LAKN7
    lensInterfaces.push_back(LensInterface(1.85f, 1.603f, -89.35f, 7.7f)); //F5
    lensInterfaces.push_back(LensInterface(3.52f, 0.0f, 580.38f, 9.55f)); //air
    lensInterfaces.push_back(LensInterface(1.85f, 1.643f, -80.63f, 13.07f)); //BAF9
    lensInterfaces.push_back(LensInterface(4.18f, 0.0f, 28.34f, 14.92f)); //air
    lensInterfaces.push_back(LensInterface(3.0f, 0.0f, std::numeric_limits<float>::infinity(), 19.1f)); //air (iris aperture)
    lensInterfaces.push_back(LensInterface(1.85f, 1.581f, std::numeric_limits<float>::infinity(), 22.1f)); //LF5
    lensInterfaces.push_back(LensInterface(7.27f, 1.694f, 32.19f, 23.95f)); //LAK13
    lensInterfaces.push_back(LensInterface(81.857f, 0.0f, -52.99f, 31.22f)); //air

    return LensSystem(10.f, 10.f, lensInterfaces);

}

class Application {
public:
    Application()
        : m_window("Ray Transfer Matrices", glm::ivec2(1024, 1024), OpenGLVersion::GL45)
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
        int dummyInteger = 0; // Initialized to 0
        LensSystem lensSystem = generateExampleLens();

        //RayTransferMatrixBuilder rtmb = RayTransferMatrixBuilder();
        //float initialPos = 0.0f;
        //glm::vec2 initialRay = glm::vec2(0.0f, toRad(10.0f)); // First term is displacement to the optical axis, second term is the angle\

        //float di = 0.5f;
        //glm::mat2x2 tMat = rtmb.getTranslationMatrix(di);
        //glm::vec2 transformedRay = tMat * initialRay;
        //float transformedPos = initialPos + di;

        //LineDrawer initialRayLine = raytoLine(initialPos, initialRay);
        //LineDrawer transformedRayLine = raytoLine(transformedPos, transformedRay);

        //std::vector<glm::vec3> curve;
        //for (float y = -1.0f; y <= 1.0f; y += 0.01f) {
        //    float x = 0.2f * (y * y - 1.0f); // Quadratic function to create a bowl shape
        //    curve.push_back(glm::vec3(x, y, 0.f));
        //}
        //LineDrawer curvedLine(curve);

        //LOOP
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Window");
            ImGui::InputInt("This is an integer input", &dummyInteger); // Use ImGui::DragInt or ImGui::DragFloat for larger range of numbers.
            ImGui::Text("Value is: %i", dummyInteger); // Use C printf formatting rules (%i is a signed integer)
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);
            ImGui::End();

            // Update Projection Matrix
            glm::mat4 projection = glm::ortho(m_left_clipping_plane, m_right_clipping_plane, m_bottom_clipping_plane, m_top_clipping_plane);

            // Clear the screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

            //const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            //// Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            //// https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            //const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));
            
            m_defaultShader.bind();

            //initialRayLine.drawLine(projection);
            //transformedRayLine.drawLine(projection);
            //curvedLine.drawLine(projection);
            lensSystem.drawLensSystem(projection);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        switch (key)
        {
        case GLFW_KEY_A:
            m_left_clipping_plane -= 1.f;
            m_right_clipping_plane -= 1.f;
            break;
        case GLFW_KEY_D:
            m_left_clipping_plane += 1.f;
            m_right_clipping_plane += 1.f;
            break;
        case GLFW_KEY_S:
            m_bottom_clipping_plane -= 1.f;
            m_top_clipping_plane -= 1.f;
            break;
        case GLFW_KEY_W:
            m_bottom_clipping_plane += 1.f;
            m_top_clipping_plane += 1.f;
            break;
        case GLFW_KEY_Q:
            m_left_clipping_plane -= 1.f;
            m_right_clipping_plane += 1.f;
            m_bottom_clipping_plane -= 1.f;
            m_top_clipping_plane += 1.f;
            break;
        case GLFW_KEY_E:
            if (abs(m_left_clipping_plane - m_right_clipping_plane) > 2.f) {
                m_left_clipping_plane += 1.f;
                m_right_clipping_plane -= 1.f;
                m_bottom_clipping_plane += 1.f;
                m_top_clipping_plane -= 1.f;
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

    float m_left_clipping_plane = -1.0f;
    float m_right_clipping_plane = 1.0f;
    float m_bottom_clipping_plane = -1.0f;
    float m_top_clipping_plane = 1.0f;
};

int main()
{
    Application app;
    app.update();

    return 0;
}

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
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <cmath>
#include "ray_transfer_matrices.h"

float toRad(float degrees) {
    return degrees * 3.141593f / 180.0f;
}

void drawRayExample() {
    // Set the background color to white
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the line color to black
    glColor3f(0.0f, 0.0f, 0.0f);

    // Set the point size
    glPointSize(5.0);

    glBegin(GL_POINTS);
    glVertex2f(0.0f, 0.0f); // Draw a point at the origin
    glEnd();

    RayTransferMatrixBuilder rtmb = RayTransferMatrixBuilder();
    float initialPos = 0.0f;
    glm::vec2 initialRay = glm::vec2(0.0f, toRad(10.0f)); // First term is displacement to the optical axis, second term is the angle

    //DRAW RAY AT INITIAL POS
    glBegin(GL_LINES);
    glVertex2f(initialPos, initialRay.x);
    glVertex2f(initialPos + cos(initialRay.y), initialRay.x + sin(initialRay.y));
    glEnd();

    // Set the line color to black
    glColor3f(1.0f, 0.0f, 0.0f);

    float di = 0.1f;
    glm::mat2x2 tMat = rtmb.getTranslationMatrix(di);
    glm::vec2 transformedRay = tMat * initialRay;
    float transformedPos = initialPos + di;

    glBegin(GL_POINTS);
    glVertex2f(transformedPos, 0.0f);
    glEnd();

    //DRAW TRANSFORMED RAY
    glBegin(GL_LINES);
    glVertex2f(transformedPos, transformedRay.x);
    glVertex2f(transformedPos + cos(transformedRay.y), transformedRay.x + sin(transformedRay.y));
    glEnd();


    //glBegin(GL_LINE_STRIP);
    //for (float y = -1.0f; y <= 1.0f; y += 0.01f) {
    //    float x = 0.2f * (y * y - 1.0f); // Quadratic function to create a bowl shape
    //    glVertex2f(x, y);
    //}
    //glEnd();

    glFlush();
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
        int dummyInteger = 0; // Initialized to 0
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

            // Clear the screen
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

            //const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            //// Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            //// https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            //const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));
            GLfloat lineVertices[] = {
                0.0f, 0.0f, 0.0f,  // First point (x, y, z)
                1.0f, 1.0f, 0.0f   // Second point (x, y, z)
            };
            
            GLuint m_vaoLine;
            GLuint m_vboLine;

            glGenVertexArrays(1, &m_vaoLine);
            glGenBuffers(1, &m_vboLine);
            glBindVertexArray(m_vaoLine);

            glBindBuffer(GL_ARRAY_BUFFER, m_vboLine);
            glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            
            m_defaultShader.bind();
            const glm::vec2 screenPos = glm::vec2(0.5f, 0.0f);

            glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(projection));
            glUniform2fv(1, 1, glm::value_ptr(screenPos));
            glUniform3fv(2, 1, glm::value_ptr(glm::vec3(1.f, 1.f, 1.f)));

            glDrawArrays(GL_LINES, 0, 2);

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
        std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    Window m_window;

    // Shader for default rendering
    Shader m_defaultShader;

    bool m_useMaterial{ false };

    // Projection and view matrices for you to fill in and use
    //glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f);
    //glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    //glm::mat4 m_modelMatrix{ 1.0f };
};

int main()
{
    Application app;
    app.update();

    return 0;
}

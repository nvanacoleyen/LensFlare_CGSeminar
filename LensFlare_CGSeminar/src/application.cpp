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


void drawLines() {
    // Set the background color to white
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set the line color to black
    glColor3f(0.0f, 0.0f, 0.0f);

    RayTransferMatrixBuilder rtmb = RayTransferMatrixBuilder();
    glm::vec2 initialPos = glm::vec2(0.5f, 5.0f);
    glm::mat2x2 tMat = rtmb.getTranslationMatrix(2.0f);
    glm::vec2 translatedPos = tMat * initialPos;

    // Draw a straight vertical line
    glBegin(GL_LINES);
    glVertex2f(initialPos.x, initialPos.y);
    glVertex2f(translatedPos.x, translatedPos.y);
    glEnd();

    //glBegin(GL_LINES);
    //glVertex2f(0.0f, 0.5f);
    //glVertex2f(0.0f, -0.5f);
    //glEnd();

    //glBegin(GL_LINE_STRIP);
    //for (float y = -1.0f; y <= 1.0f; y += 0.01f) {
    //    float x = 0.2f * (y * y - 1.0f); // Quadratic function to create a bowl shape
    //    glVertex2f(x, y);
    //}
    //glEnd();

    glFlush();
}

int main() {
    if (!glfwInit()) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 960, "OpenGL Lines", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        drawLines();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

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

int main() {
    if (!glfwInit()) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Ray Transfer Matrix", NULL, NULL);
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
        drawRayExample();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

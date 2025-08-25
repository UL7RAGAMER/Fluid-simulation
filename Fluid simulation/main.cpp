#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "Shader.h" // Make sure this can handle compute shaders now
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void ComputeCircleVertices(std::vector<float>& vertices, int numSegments, float radius);
float random_float(float min, float max);
// --- Global variable for gravity strength, controllable by ImGui
float gravityStrength = 0.5f;
struct mouse_pos {
    double x;
    double y;
	bool pressed;
};


int main()
{
    GLFWwindow* window;
    if (!glfwInit())
        return -1;

    // --- NEW --- Request a newer OpenGL context to support compute shaders
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(800, 800, "Compute Shader Gravity", NULL, NULL);
    glfwMakeContextCurrent(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    if (glewInit() != GLEW_OK) {
        std::cout << "Error!" << std::endl;
        return -1;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    std::vector <float> vertices;
    ComputeCircleVertices(vertices, 32, 0.005f);
    const int particleCount = 500000;

    // --- We still generate initial data on the CPU ---
    std::vector<glm::vec2> initialPositions(particleCount);
    std::vector<glm::vec2> initialVelocities(particleCount, glm::vec2(0.0f, 0.0f));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dis(-0.9f, 0.9f);
    std::uniform_real_distribution<float> vel_dis(-0.2f, 0.2f);

    for (int i = 0; i < particleCount; ++i) {
        initialPositions[i] = glm::vec2(pos_dis(gen), pos_dis(gen));
        initialVelocities[i] = glm::vec2(vel_dis(gen), vel_dis(gen));
    }

    // --- VAO and VBO for drawing the circle mesh  ---
    unsigned int circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int positionSSBO, velocitySSBO;

    // Position SSBO
    glGenBuffers(1, &positionSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * particleCount, &initialPositions[0], GL_DYNAMIC_DRAW);

    // Velocity SSBO
    glGenBuffers(1, &velocitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * particleCount, &initialVelocities[0], GL_DYNAMIC_DRAW);

    // The position and velocity buffer is now used for both physics calculation and rendering.
    glBindBuffer(GL_ARRAY_BUFFER, positionSSBO);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, velocitySSBO);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced vertex attribute.


    glVertexAttribDivisor(1, 1); // Enable instancing

    glBindVertexArray(0);

    // --- Load our two separate shader programs ---
    Shader renderShader("Basic.shader");
    Shader computeShader("physics.comp");

    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        int isMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float mouseX = (float)(xpos / width) * 2.0f - 1.0f;
        float mouseY = 1.0f - (float)(ypos / height) * 2.0f; // Y is inverted
        glUseProgram(computeShader.shader_obj);

        // 2. Bind the SSBOs to the correct binding points (0 and 1)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocitySSBO);

        // 3. Set the uniforms for the compute shader
        glUniform1f(glGetUniformLocation(computeShader.shader_obj, "deltaTime"), deltaTime);
        glUniform1f(glGetUniformLocation(computeShader.shader_obj, "gravity"), gravityStrength);
        glUniform1f(glGetUniformLocation(computeShader.shader_obj, "u_time"), (float)glfwGetTime());
        glUniform1i(glGetUniformLocation(computeShader.shader_obj, "is_mouse_pressed"), isMouseDown);
        glUniform2f(glGetUniformLocation(computeShader.shader_obj, "mouse_pos"), mouseX, mouseY);

        // 4. Dispatch the compute shader!
        glDispatchCompute((particleCount + 128 - 1) / 128, 1, 1);
        // 5. IMPORTANT: Add a memory barrier.

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


        // --- RENDER STEP ---
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Activate the rendering shader program
        glUseProgram(renderShader.shader_obj);
        glBindVertexArray(circleVAO);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, vertices.size() / 3, particleCount);

        // --- IMGUI STEP ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controls");
        ImGui::SliderFloat("Gravity", &gravityStrength, 0.0f, 5.0f);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{

}


void ComputeCircleVertices(std::vector<float>& vertices, int numSegments, float radius)
{
    vertices.clear();
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    for (int i = 0; i <= numSegments; ++i)
    {
        float angle = 2.0f * 3.1415926f * float(i) / float(numSegments);
        float x = radius * cosf(angle);
        float y = radius * sinf(angle);
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);
    }
}

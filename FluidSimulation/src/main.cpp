#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Simulation.h"
#include "Renderer.h"
#include <algorithm>

// Globals for callbacks
int g_ViewportX = 0;
int g_ViewportY = 0;
int g_ViewportWidth = 1920;
int g_ViewportHeight = 1080;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    g_ViewportX = 0;
    g_ViewportY = 0;
    g_ViewportWidth = width;
    g_ViewportHeight = height;
    glViewport(0, 0, width, height);
}

int main()
{
    // --- Window Init ---
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Fluid Simulation", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);

    // --- GLEW Init ---
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // --- ImGui Init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    // --- Simulation & Renderer Init ---
    Simulation sim;
    Renderer renderer;

    sim.Init(50000, 50000); // Max 50000, Initial 50000
    renderer.Init();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initial Resize Call
    int initialWidth, initialHeight;
    glfwGetFramebufferSize(window, &initialWidth, &initialHeight);
    framebuffer_size_callback(window, initialWidth, initialHeight);
    int initialViewportMin = std::min(initialWidth, initialHeight);

    float lastFrame = 0.0f;

    // --- Main Loop ---
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- ImGui Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Input Processing ---
        bool isMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) isMouseDown = false;

        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        float normX = (float)(xpos - g_ViewportX) / (float)g_ViewportWidth;
        float normY = (float)(ypos - g_ViewportY) / (float)g_ViewportHeight;

        float viewportMin = (float)std::min(g_ViewportWidth, g_ViewportHeight);
        float simScale = viewportMin / (float)initialViewportMin;
        float simScaleClamped = std::max(0.5f, std::min(simScale, 2.0f));
        float simBoundaryLimit = 1.0f * simScaleClamped;

        float mouseX = normX * 2.0f * simScaleClamped - 1.0f * simScaleClamped;
        float mouseY = 1.0f * simScaleClamped - normY * 2.0f * simScaleClamped;

        if (normX < 0.0f || normX > 1.0f || normY < 0.0f || normY > 1.0f) isMouseDown = false;
        if (mouseX < -simBoundaryLimit || mouseX > simBoundaryLimit || mouseY < -simBoundaryLimit || mouseY > simBoundaryLimit) isMouseDown = false;

        // --- Simulation Update ---
        sim.Update(deltaTime, currentFrame, isMouseDown, mouseX, mouseY, simBoundaryLimit);

        // --- Render ---
        float displayAspect = (float)g_ViewportWidth / (float)g_ViewportHeight;
        renderer.Render(sim.GetParticleCount(), sim.GetPositionSSBO(), sim.GetVelocitySSBO(), sim.GetDensitySSBO(), sim.GetPressureSSBO(), simBoundaryLimit, displayAspect);

        // --- UI ---
        ImGui::Begin("Controls");
        ImGui::SliderFloat("Gravity", &sim.gravityStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("Rest Density", &sim.restDensity, 0.0f, 10.0f);
        ImGui::SliderFloat("Gas Constant", &sim.gasConstant, 0.0f, 10.0f);
        ImGui::SliderFloat("Boundary Stiffness", &sim.boundaryStiffness, 500.0f, 10000.0f);
        ImGui::SliderFloat("Boundary Damping", &sim.boundaryDamping, 0.1f, 1.0f);
        ImGui::SliderFloat("Viscosity", &sim.viscosity, 0.0f, 2.0f);
        ImGui::SliderFloat("Viscosity Const", &sim.viscosityConstant, 0.0f, 2.0f);
        ImGui::SliderFloat("Pressure Multiplier", &sim.pressureMultiplier, 0.0f, 0.01f);
        ImGui::SliderFloat("Surface Tension", &sim.surfaceTension, 0.0f, 1000.0f);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        static int particleSliderCount = sim.GetParticleCount();
        ImGui::SliderInt("Particle Count", &particleSliderCount, 1, sim.GetMaxParticles(), "%d", ImGuiSliderFlags_Logarithmic);

        if (ImGui::IsItemDeactivatedAfterEdit()) {
            sim.UpdateParticleCount(particleSliderCount);
        }

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}

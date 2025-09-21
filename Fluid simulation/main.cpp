#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <cmath> // Required for std::sqrt
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Corrected from .hh to .h
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "Shader.h" // Make sure this can handle compute shaders now

#define log(x) std::cout << x << std::endl

// --- Function Prototypes ---
void ComputeCircleVertices(std::vector<float>& vertices, int numSegments, float radius);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// --- NEW --- Global viewport dimensions for mouse coordinate correction and aspect ratio
int g_ViewportX = 0;
int g_ViewportY = 0;
int g_ViewportWidth = 2560;
int g_ViewportHeight = 1351;

// --- Global variable for gravity strength, controllable by ImGui
const unsigned int GRID_DIM = 64; // A 64x64 grid
const unsigned int NUM_GRID_CELLS = GRID_DIM * GRID_DIM;
const float CELL_SIZE = 2.0f / GRID_DIM; // Domain is [-1, 1],

int main()
{
    GLFWwindow* window;
    if (!glfwInit())
        return -1;
    // --- NEW --- Request a newer OpenGL context to support compute shaders
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(2560, 1351, "Resizable Compute Shader Gravity", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    // --- Register the resize callback function ---
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    if (glewInit() != GLEW_OK) {
        std::cout << "Error!" << std::endl;
        return -1;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSwapInterval(0);
    // --- NEW --- Set initial viewport correctly by calling the callback once
    int initialWidth, initialHeight;
    glfwGetFramebufferSize(window, &initialWidth, &initialHeight);
    framebuffer_size_callback(window, initialWidth, initialHeight);
    int initialViewportMin = std::min(initialWidth, initialHeight);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

    std::vector <float> vertices;
    ComputeCircleVertices(vertices, 32, 0.002f);
    const int particleCount = 50000;

    // --- We still generate initial data on the CPU ---
    std::vector<glm::vec2> initialPositions(particleCount);
    std::vector<glm::vec2> initialVelocities(particleCount, glm::vec2(0.0f, 0.0f));


    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dis(-0.9f, 0.9f);
    std::uniform_real_distribution<float> vel_dis(-0.2f, 0.2f);


    // --- Grid Configuration ---
    // Set the number of columns for your grid. Using the square root creates a roughly square layout.
    int numColumns = static_cast<int>(std::sqrt(particleCount));
    float spacing = 0.05f;
    // Calculate an offset to center the entire grid at the origin (0,0).
    float offset = (numColumns - 1) * spacing / 2.0f;

    // --- Initialization Loop ---
    for (int i = 0; i < particleCount; ++i) {
        // Calculate the (column, row) position for the current particle.
        int col = i % numColumns;
        int row = i / numColumns;

        // Set the initial position based on the grid coordinates, spacing, and centering offset.
        initialPositions[i] = glm::vec2(col * spacing - offset, row * spacing - offset);
    }
    // --- NEW --- Setup for drawing the grid visualization
    unsigned int gridVAO, gridVBO;
    float quadVertices[] = {
        // positions      
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
    };
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(gridVAO, 0); // FIX: Use two-argument version
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // --- VAO and VBO for drawing the circle mesh  ---
    unsigned int circleVAO, circleVBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(circleVAO, 0); // FIX: Use two-argument version
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    unsigned int positionSSBO, velocitySSBO;
    unsigned int densitySSBO, cellCountsSSBO, pressureSSBO;

    // Position SSBO
    glGenBuffers(1, &positionSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * particleCount, initialPositions.data(), GL_DYNAMIC_DRAW);

    // Velocity SSBO
    glGenBuffers(1, &velocitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * particleCount, initialVelocities.data(), GL_DYNAMIC_DRAW);

    // Density SSBO (stores the calculated density for each particle)
    glGenBuffers(1, &densitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, densitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * particleCount, NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &pressureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pressureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * particleCount, NULL, GL_DYNAMIC_DRAW);

    // Cell Counts SSBO (stores particle count for each grid cell)
    glGenBuffers(1, &cellCountsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellCountsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * NUM_GRID_CELLS, NULL, GL_DYNAMIC_DRAW);

    // Bind particle instance data
    glBindVertexArray(circleVAO);
    // Position
    glBindBuffer(GL_ARRAY_BUFFER, positionSSBO);
    glEnableVertexAttribArray(circleVAO, 1); // FIX: Use two-argument version
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(1, 1);
    // Velocity
    glBindBuffer(GL_ARRAY_BUFFER, velocitySSBO);
    glEnableVertexAttribArray(circleVAO, 2); // FIX: Use two-argument version
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(2, 1);
    // Density
    glBindBuffer(GL_ARRAY_BUFFER, densitySSBO);
    glEnableVertexAttribArray(circleVAO, 3); // FIX: Use two-argument version
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glVertexAttribDivisor(3, 1);
    // Pressure
    glBindBuffer(GL_ARRAY_BUFFER, pressureSSBO);
    glEnableVertexAttribArray(circleVAO, 4); // FIX: Use two-argument version
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glVertexAttribDivisor(4, 1);


    glBindVertexArray(0);

    // --- Load our two separate shader programs ---
    Shader renderShader("Basic.shader");
    Shader gridVisShader("grid_vis.shader");

    Shader physicsUpdateShader("physics.comp");
    Shader densityShader("density.comp");
    Shader gridClearShader("grid_clear.comp");
    Shader gridCountShader("grid_count.comp");
    float lastFrame = 0.0f;

    float forceMultiplier = 1.0 / 200.0;
    float separationStiffness = 400.0;
    float viscosity = 0.1f;
    float particleMass = 0.01f;
    float smoothingRadius = 0.2f;
    float viscosityConstant = 0.25f;
    float gasConstant = 1.0f;
    float restDensity = 10.0f;
    float boundaryStiffness = 1200.0f;
    float boundaryDamping = 0.75f;
    float pressure_multipiler = 0.01f;
    float gravityStrength = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        // compute a scale from current viewport to the original initial viewport
        float viewportMin = (float)std::min(g_ViewportWidth, g_ViewportHeight);
        float initMin = (float)initialViewportMin;
        float simScale = viewportMin / initMin;               // 1.0 at original size, <1 if smaller, >1 if larger

        // clamp scale to avoid extreme domains
        float simScaleClamped = std::max(0.5f, std::min(simScale, 2.0f)); // adjust min/max as you like

        // baseline domain half-size is 1.0 at original size; scale it
        float simBoundaryLimit = 1.0f * simScaleClamped;
        // --- Start of Frame: Poll events and calculate delta time ---
        glfwPollEvents();

        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- IMGUI: Start new frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Input & Mouse Coordinate Processing ---
        int isMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
            isMouseDown = 0; // Prevent interaction if mouse is over an ImGui window
        }
        
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // 1. Convert cursor position directly to normalized viewport coordinates [0, 1]
        // g_ViewportX and g_ViewportY are the offsets of the viewport in framebuffer pixels
        // g_ViewportWidth and g_ViewportHeight are the dimensions of the viewport in framebuffer pixels
        float normX = (float)(xpos - g_ViewportX) / (float)g_ViewportWidth;
        float normY = (float)(ypos - g_ViewportY) / (float)g_ViewportHeight;

        // 2. Convert normalized viewport coordinates to OpenGL's Normalized Device Coordinates [-1, 1]
        float mouseX = normX * 2.0f* simScaleClamped - 1.0f* simScaleClamped;
        float mouseY = 1.0f * simScaleClamped - normY * 2.0f * simScaleClamped; // Invert Y-axis for OpenGL


        // 3. Disable mouse interaction if the cursor is outside the viewport (in the black bars)
        if (normX < 0.0f || normX > 1.0f || normY < 0.0f || normY > 1.0f) {
            isMouseDown = 0;
        }

        if (mouseX < -1.0f || mouseX > 1.0f || mouseY < -1.0f || mouseY > 1.0f) {
            isMouseDown = 0; // Don't interact if mouse is outside the sim area
        }
		//log("Mouse NDC: (" << mouseX << ", " << mouseY << "), isMouseDown: " << isMouseDown);
        

        // 1. CLEAR: Reset the grid cell counters to zero
        glUseProgram(gridClearShader.shader_obj);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsSSBO);
        glDispatchCompute((NUM_GRID_CELLS + 127) / 128, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Wait for clear to finish

        // 2. COUNT: Assign particles to grid cells and count them
        glUseProgram(gridCountShader.shader_obj);
        glUniform1ui(glGetUniformLocation(gridCountShader.shader_obj, "gridDim"), GRID_DIM);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO); // READ positions
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsSSBO); // WRITE counts

        glDispatchCompute((particleCount + 127) / 128, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Wait for counting to finish

        // 3. CALCULATE: Calculate density
        glUseProgram(densityShader.shader_obj);
        glUniform1ui(glGetUniformLocation(densityShader.shader_obj, "particleCount"), particleCount);
        glUniform1f(glGetUniformLocation(densityShader.shader_obj, "particleMass"), particleMass);
        glUniform1f(glGetUniformLocation(densityShader.shader_obj, "smoothingRadius"), smoothingRadius);
        glUniform1f(glGetUniformLocation(densityShader.shader_obj, "gasConstant"), gasConstant);
        glUniform1f(glGetUniformLocation(densityShader.shader_obj, "restDensity"), restDensity);

        // Bind buffers for the density pass
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, densitySSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pressureSSBO);

        glDispatchCompute((particleCount + 127) / 128, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Ensure density pass is complete

        // 4. FORCE PASS: Apply forces and integrate particle positions
        glUseProgram(physicsUpdateShader.shader_obj);
        glUniform1ui(glGetUniformLocation(physicsUpdateShader.shader_obj, "particleCount"), particleCount);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "deltaTime"), deltaTime > 0.008f ? 0.008f : deltaTime); // Clamp delta time
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "gravity"), gravityStrength);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "u_time"), currentFrame);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "particleMass"), particleMass);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "smoothingRadius"), smoothingRadius);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "viscosityConstant"), viscosityConstant);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "is_mouse_pressed"), isMouseDown);
        glUniform2f(glGetUniformLocation(physicsUpdateShader.shader_obj, "mouse_pos"), mouseX, mouseY);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "boundaryStiffness"), boundaryStiffness);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "boundaryDamping"), boundaryDamping);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "pressure_multipiler"), pressure_multipiler);


        // compute boundary_radius relative to smoothingRadius (keeps visual size consistent)
        float simBoundaryRadius = std::max(0.005f, smoothingRadius * 0.05f); // min radius 0.005

        // upload to shader
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "boundary_limit"), simBoundaryLimit);
        glUniform1f(glGetUniformLocation(physicsUpdateShader.shader_obj, "boundary_radius"), simBoundaryRadius);

        // Bind buffers for the force pass
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocitySSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, densitySSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, pressureSSBO);

        glDispatchCompute((particleCount + 127) / 128, 1, 1);
        // --- FIX --- Use a barrier that ensures compute shader writes are visible to the vertex rendering stage.
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        // --- RENDER STEP ---
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Activate the rendering shader program
        glUseProgram(renderShader.shader_obj);
        glBindVertexArray(circleVAO);
        glUniform1f(glGetUniformLocation(renderShader.shader_obj, "simBoundaryLimit"), simBoundaryLimit);
        float displayAspect = (float)g_ViewportWidth / (float)g_ViewportHeight;
        glUniform1f(glGetUniformLocation(renderShader.shader_obj, "displayAspect"), displayAspect);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, (GLsizei)(vertices.size() / 3), particleCount);

        // --- IMGUI STEP ---
        ImGui::Begin("Controls");
        ImGui::SliderFloat("Gravity", &gravityStrength, 0.0f, 10.0f);
        ImGui::SliderFloat("restDensity", &restDensity, 0.0f, 10.0f);
        ImGui::SliderFloat("gasConstant", &gasConstant, 0.0f, 10.0f);
        ImGui::SliderFloat("Boundary Stiffness", &boundaryStiffness, 500.0f, 10000.0f);
        ImGui::SliderFloat("Boundary Damping", &boundaryDamping, 0.1f, 1.0f);
        ImGui::SliderFloat("viscosity", &viscosity, 0.0f, 2.0f);
        ImGui::SliderFloat("viscosityCOnst", &viscosityConstant, 0.0f, 2.0f);
        ImGui::SliderFloat("pressure_multipiler", &pressure_multipiler, 0.0f, 0.01f);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    g_ViewportX = 0;
    g_ViewportY = 0;
    g_ViewportWidth = width;
    g_ViewportHeight = height;

    // Use full window
    glViewport(0, 0, width, height);

    log("Resized to " << width << "x" << height << ", Viewport: " << g_ViewportWidth << "x" << g_ViewportHeight);
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


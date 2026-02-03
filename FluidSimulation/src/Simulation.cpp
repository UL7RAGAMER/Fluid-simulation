#include "Simulation.h"
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <vector>

Simulation::Simulation()
    : maxParticles(0), currentParticleCount(0),
      positionSSBO(0), velocitySSBO(0), densitySSBO(0), pressureSSBO(0), cellCountsSSBO(0)
{
}

Simulation::~Simulation() {
    // Resources are cleaned up by OpenGL context destruction usually,
    // but explicit cleanup could be added here.
}

void Simulation::Init(unsigned int maxParticles, unsigned int initialParticles) {
    this->maxParticles = maxParticles;
    this->currentParticleCount = initialParticles;

    // --- Generate Initial Data on CPU ---
    std::vector<glm::vec2> initialPositions(currentParticleCount);
    std::vector<glm::vec2> initialVelocities(currentParticleCount, glm::vec2(0.0f, 0.0f));

    // Grid Configuration for Initialization
    int numColumns = static_cast<int>(std::sqrt(currentParticleCount));
    float spacing = 0.05f;
    float offset = (numColumns - 1) * spacing / 2.0f;

    for (int i = 0; i < (int)currentParticleCount; ++i) {
        int col = i % numColumns;
        int row = i / numColumns;
        initialPositions[i] = glm::vec2(col * spacing - offset, row * spacing - offset);
    }

    // --- SSBO Initialization ---
    // Position SSBO
    glGenBuffers(1, &positionSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * maxParticles, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec2) * currentParticleCount, initialPositions.data());

    // Velocity SSBO
    glGenBuffers(1, &velocitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * maxParticles, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec2) * currentParticleCount, initialVelocities.data());

    // Density SSBO
    glGenBuffers(1, &densitySSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, densitySSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * maxParticles, NULL, GL_DYNAMIC_DRAW);

    // Pressure SSBO
    glGenBuffers(1, &pressureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pressureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * maxParticles, NULL, GL_DYNAMIC_DRAW);

    // Cell Counts SSBO
    glGenBuffers(1, &cellCountsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellCountsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * NUM_GRID_CELLS, NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // --- Shader Loading ---
    // Assuming shaders are in assets/shaders/ relative to working directory
    physicsUpdateShader = std::make_unique<Shader>("assets/shaders/physics.comp");
    densityShader = std::make_unique<Shader>("assets/shaders/density.comp");
    gridClearShader = std::make_unique<Shader>("assets/shaders/grid_clear.comp");
    gridCountShader = std::make_unique<Shader>("assets/shaders/grid_count.comp");
}

void Simulation::Update(float deltaTime, float currentFrame, bool isMouseDown, float mouseX, float mouseY, float simBoundaryLimit) {
    // 1. CLEAR: Reset the grid cell counters to zero
    glUseProgram(gridClearShader->shader_obj);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsSSBO);
    glDispatchCompute((NUM_GRID_CELLS + 127) / 128, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 2. COUNT: Assign particles to grid cells and count them
    glUseProgram(gridCountShader->shader_obj);
    glUniform1ui(glGetUniformLocation(gridCountShader->shader_obj, "gridDim"), GRID_DIM);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO); // READ positions
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellCountsSSBO); // WRITE counts

    glDispatchCompute((currentParticleCount + 127) / 128, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 3. CALCULATE: Calculate density
    glUseProgram(densityShader->shader_obj);
    glUniform1ui(glGetUniformLocation(densityShader->shader_obj, "particleCount"), currentParticleCount);
    glUniform1f(glGetUniformLocation(densityShader->shader_obj, "particleMass"), particleMass);
    glUniform1f(glGetUniformLocation(densityShader->shader_obj, "smoothingRadius"), smoothingRadius);
    glUniform1f(glGetUniformLocation(densityShader->shader_obj, "gasConstant"), gasConstant);
    glUniform1f(glGetUniformLocation(densityShader->shader_obj, "restDensity"), restDensity);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, densitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pressureSSBO);

    glDispatchCompute((currentParticleCount + 127) / 128, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 4. FORCE PASS: Apply forces and integrate particle positions
    glUseProgram(physicsUpdateShader->shader_obj);
    glUniform1ui(glGetUniformLocation(physicsUpdateShader->shader_obj, "particleCount"), currentParticleCount);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "deltaTime"), deltaTime > 0.008f ? 0.008f : deltaTime);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "gravity"), gravityStrength);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "u_time"), currentFrame);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "particleMass"), particleMass);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "smoothingRadius"), smoothingRadius);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "viscosityConstant"), viscosityConstant);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "is_mouse_pressed"), isMouseDown);
    glUniform2f(glGetUniformLocation(physicsUpdateShader->shader_obj, "mouse_pos"), mouseX, mouseY);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "boundaryStiffness"), boundaryStiffness);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "boundaryDamping"), boundaryDamping);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "pressure_multipiler"), pressureMultiplier);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "surfaceTension"), surfaceTension);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "surfaceThreshold"), surfaceThreshold);

    float simBoundaryRadius = smoothingRadius * 0.000005f;
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "boundary_limit"), simBoundaryLimit);
    glUniform1f(glGetUniformLocation(physicsUpdateShader->shader_obj, "boundary_radius"), simBoundaryRadius);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, velocitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, densitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, pressureSSBO);

    glDispatchCompute((currentParticleCount + 127) / 128, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

void Simulation::UpdateParticleCount(int newCount) {
    if (newCount == (int)currentParticleCount) return;
    if (newCount > (int)maxParticles) newCount = maxParticles;

    if (newCount > (int)currentParticleCount) {
        int numToAdd = newCount - currentParticleCount;
        std::vector<glm::vec2> newPositions(numToAdd);
        std::vector<glm::vec2> newVelocities(numToAdd, glm::vec2(0.0f));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        for (int i = 0; i < numToAdd; ++i) {
            float radius = 0.2f * std::sqrt(dist(gen));
            float angle = 2.0f * 3.1415926f * dist(gen);
            newPositions[i] = glm::vec2(radius * cos(angle), radius * sin(angle));
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * currentParticleCount, sizeof(glm::vec2) * numToAdd, newPositions.data());

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocitySSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::vec2) * currentParticleCount, sizeof(glm::vec2) * numToAdd, newVelocities.data());

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        std::cout << "Added " << numToAdd << " particles." << std::endl;
    }

    currentParticleCount = newCount;
    std::cout << "Particle count set to " << currentParticleCount << std::endl;
}

#pragma once
#include <vector>
#include <memory>
#include <GL/glew.h>
#include "glm.hpp"
#include "Shader.h"

class Simulation {
public:
    Simulation();
    ~Simulation();

    void Init(unsigned int maxParticles, unsigned int initialParticles);
    void Update(float deltaTime, float currentFrame, bool isMouseDown, float mouseX, float mouseY, float simBoundaryLimit);
    void UpdateParticleCount(int newCount);

    unsigned int GetPositionSSBO() const { return positionSSBO; }
    unsigned int GetVelocitySSBO() const { return velocitySSBO; }
    unsigned int GetDensitySSBO() const { return densitySSBO; }
    unsigned int GetPressureSSBO() const { return pressureSSBO; }
    unsigned int GetParticleCount() const { return currentParticleCount; }
    unsigned int GetMaxParticles() const { return maxParticles; }

    // Simulation Parameters
    float gravityStrength = 9.8f;
    float restDensity = 10.0f;
    float gasConstant = 1.0f;
    float boundaryStiffness = 1200.0f;
    float boundaryDamping = 0.75f;
    float viscosity = 0.1f; // Used for UI slider, might need to map to viscosityConstant if they are related
    float viscosityConstant = 0.25f;
    float pressureMultiplier = 0.01f;
    float surfaceTension = 80.0f;
    float surfaceThreshold = 1e-8;
    float smoothingRadius = 0.2f;
    float particleMass = 0.01f;

private:
    unsigned int maxParticles;
    unsigned int currentParticleCount;

    unsigned int positionSSBO;
    unsigned int velocitySSBO;
    unsigned int densitySSBO;
    unsigned int pressureSSBO;
    unsigned int cellCountsSSBO;

    std::unique_ptr<Shader> physicsUpdateShader;
    std::unique_ptr<Shader> densityShader;
    std::unique_ptr<Shader> gridClearShader;
    std::unique_ptr<Shader> gridCountShader;

    static const unsigned int GRID_DIM = 64;
    static const unsigned int NUM_GRID_CELLS = GRID_DIM * GRID_DIM;
};

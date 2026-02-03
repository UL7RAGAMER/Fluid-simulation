#pragma once
#include <vector>
#include <memory>
#include <GL/glew.h>
#include "glm.hpp"
#include "Shader.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    void Init();
    void Render(unsigned int particleCount, unsigned int posSSBO, unsigned int velSSBO, unsigned int densitySSBO, unsigned int pressureSSBO, float simBoundaryLimit, float displayAspect);

private:
    unsigned int circleVAO, circleVBO;
    std::unique_ptr<Shader> renderShader;
    std::vector<float> circleVertices;

    void ComputeCircleVertices(std::vector<float>& vertices, int numSegments, float radius);
};

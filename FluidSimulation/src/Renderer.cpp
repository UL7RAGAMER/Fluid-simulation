#include "Renderer.h"
#include <iostream>
#include <cmath>

Renderer::Renderer() : circleVAO(0), circleVBO(0) {}

Renderer::~Renderer() {
    // Cleanup if needed
}

void Renderer::Init() {
    // Increased radius from 0.001f to 0.01f so particles are visible
    ComputeCircleVertices(circleVertices, 32, 0.01f);

    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);

    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * circleVertices.size(), circleVertices.data(), GL_STATIC_DRAW);

    // Standard function takes only 1 argument: the attribute index
    glEnableVertexAttribArrayARB(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    renderShader = std::make_unique<Shader>("assets/shaders/Basic.shader");
}

void Renderer::Render(unsigned int particleCount, unsigned int posSSBO, unsigned int velSSBO, unsigned int densitySSBO, unsigned int pressureSSBO, float simBoundaryLimit, float displayAspect) {
    glBindVertexArray(circleVAO);

    // Position - Attribute 1
    glBindBuffer(GL_ARRAY_BUFFER, posSSBO);
    glEnableVertexAttribArrayARB(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(1, 1);

    // Velocity - Attribute 2
    glBindBuffer(GL_ARRAY_BUFFER, velSSBO);
    glEnableVertexAttribArrayARB(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    glVertexAttribDivisor(2, 1);

    // Density - Attribute 3
    glBindBuffer(GL_ARRAY_BUFFER, densitySSBO);
    glEnableVertexAttribArrayARB(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glVertexAttribDivisor(3, 1);

    // Pressure - Attribute 4
    glBindBuffer(GL_ARRAY_BUFFER, pressureSSBO);
    glEnableVertexAttribArrayARB(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
    glVertexAttribDivisor(4, 1);

    // Clear and Draw
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(renderShader->shader_obj);
    glUniform1f(glGetUniformLocation(renderShader->shader_obj, "simBoundaryLimit"), simBoundaryLimit);
    glUniform1f(glGetUniformLocation(renderShader->shader_obj, "displayAspect"), displayAspect);

    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, (GLsizei)(circleVertices.size() / 3), particleCount);

    glBindVertexArray(0);
}
void Renderer::ComputeCircleVertices(std::vector<float>& vertices, int numSegments, float radius) {
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

#shader vertex
#version 430 core

// ATTRIBUTE LOCATIONS MUST MATCH THE glVertexAttribPointer CALLS IN C++
layout (location = 0) in vec3 aPos;      // Circle vertex positions from VBO
layout (location = 1) in vec2 aOffset;   // Particle position (instanced from positionSSBO)
layout (location = 2) in float aDensity;  // Particle density (instanced from densitySSBO)

// Pass density to the fragment shader
out float v_density;

// in your render vertex shader (Basic.shader vertex part)
uniform float simBoundaryLimit; // same value you sent to physics.comp
uniform float displayAspect;    // g_ViewportWidth / g_ViewportHeight
layout(location = 0) in vec3 aVertex;       // circle mesh
layout(location = 1) in vec2 instancePos;   // particle position (world coords)

void main() {
    // Convert world pos -> normalized device coords
    vec2 ndc = instancePos / simBoundaryLimit; // now in [-1,1] if simBoundaryLimit==1

    // If viewport is not square and you want to keep circles round, optionally correct X:
    // ndc.x *= (displayAspect); // uncomment if needed when using full-window viewport

    // compute vertex position offset by instance
    vec2 vertexOffset = aVertex.xy; // circle mesh coordinates in local space
    // scale vertex offset so circle radius stays consistent in NDC
    // (if you created circles in NDC radius already this may be identity)
    vec2 pos = ndc + vertexOffset;

    gl_Position = vec4(pos, 0.0, 1.0);
    v_density = aDensity;

}


#shader fragment
#version 430 core
out vec4 FragColor;

in float v_density;

// This uniform is sent from main.cpp
uniform float restDensity = 1;

// Generates a color from blue (low) to red (high) based on a 0-1 value
vec3 heatmap(float v) {
    vec3 blue   = vec3(0.2, 0.5, 1.0);
    vec3 cyan   = vec3(0.0, 1.0, 1.0);
    vec3 green  = vec3(0.5, 1.0, 0.5);
    vec3 yellow = vec3(1.0, 1.0, 0.0);
    vec3 red    = vec3(1.0, 0.0, 0.0);

    // Mix between the colors at different points in the gradient
    vec3 color = mix(blue,   cyan,   smoothstep(0.0,  0.25, v));
         color = mix(color,  green,  smoothstep(0.25, 0.5,  v));
         color = mix(color,  yellow, smoothstep(0.5,  0.75, v));
         color = mix(color,  red,    smoothstep(0.75, 1.0,  v));
         
    return color;
}

void main()
{
    // Normalize the particle's density relative to the fluid's rest density.
    // A density equal to restDensity will be 0.5 (green).
    // A density twice the restDensity will be 1.0 (red).
    float normalizedDensity = v_density / (restDensity * 2.0);

    FragColor = vec4(heatmap(normalizedDensity * 4.0f), 1.0);
}

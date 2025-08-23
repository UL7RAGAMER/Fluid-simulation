#shader vertex
#version 430 core

// Vertex position of the base circle mesh (e.g., from -1 to 1)
layout (location = 0) in vec3 aPos;

// Per-instance position offset, read from the position SSBO
layout (location = 1) in vec2 offset;

// Per-instance velocity, read from the velocity SSBO
layout (location = 2) in vec2 vel;

// Pass velocity to the fragment shader
out vec2 fVel;
void main()
{
    // Final position is the base mesh vertex position plus the instance's unique offset
    gl_Position = vec4(aPos.xy + offset, aPos.z, 1.0);
    fVel = vel;
}


#shader fragment
#version 430 core

out vec4 FragColor;

// Velocity received from the vertex shader
in vec2 fVel;

void main()
{
    float speed = length(fVel);


    vec3 color = vec3(smoothstep(0.0, 0.8, speed), 0.2, smoothstep(0.8, 0.0, speed));

    FragColor = vec4(color, 1.0f);
}

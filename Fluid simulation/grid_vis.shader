#shader vertex
#version 430 core
layout (location = 0) in vec3 aPos;

out vec2 v_worldPos;

void main()
{
    // The vertex position is already in clip space for a fullscreen quad
    gl_Position = vec4(aPos, 1.0);
    // Pass the position so the fragment shader knows its world coordinates
    v_worldPos = aPos.xy;
}


#shader fragment
#version 430 core
out vec4 FragColor;

in vec2 v_worldPos;

uniform uint gridDim;

void main()
{
    // --- Calculate distance to nearest grid line ---
    // Convert world position [-1, 1] to a fractional grid coordinate
    vec2 gridCoords = (v_worldPos + 1.0) / 2.0 * gridDim;

    // The fractional part is the coordinate *inside* one cell (from 0.0 to 1.0)
    vec2 in_cell_pos = fract(gridCoords);
    
    // Find the distance to the closest vertical and horizontal edge
    float dist_x = min(in_cell_pos.x, 1.0 - in_cell_pos.x);
    float dist_y = min(in_cell_pos.y, 1.0 - in_cell_pos.y);

    // The distance to the nearest line is the smaller of the two
    float dist_to_line = min(dist_x, dist_y);

    // --- Create a smooth, anti-aliased line ---
    // Use smoothstep to create a line that fades out gently at the edges
    // The first value (0.0) is the center of the line.
    // The second value (0.02) controls the line's thickness and softness.
    float line_alpha = 1.0 - smoothstep(0.0, 0.02, dist_to_line);

    // Make the line more prominent if it's a major grid line (e.g., every 8th line)
    float major_line_boost = 0.0;
    if (int(gridCoords.x) % 8 == 0 || int(gridCoords.y) % 8 == 0) {
        major_line_boost = 0.3;
    }

    // Final color: a dark gray line, with its alpha determined by the calculated line value
    FragColor = vec4(0.4, 0.4, 0.4, line_alpha * (0.4 + major_line_boost));
}
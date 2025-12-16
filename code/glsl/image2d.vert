#version 450

// Vertex inputs - 2D vertices are already in NDC coordinates
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord;

void main()
{
    // Vertices are already in NDC space, just pass through
    gl_Position = vec4(inPosition, 0.0, 1.0);
    outTexCoord = inTexCoord;
}

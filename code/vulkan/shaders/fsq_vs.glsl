#version 450 core

// Full-screen quad vertex shader for 2D rendering

layout(binding = 0) uniform ViewVSBlock
{
    mat4 u_projectionMatrix;
    mat4 u_modelViewMatrix;
    vec2 u_depthRange;
} u_viewVS;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texCoord;

layout(location = 0) out vec2 v_texCoord;

void main()
{
    // Convert 0..1 quad to clip space
    vec2 clipPos = in_position * 2.0 - 1.0;
    clipPos.y = -clipPos.y; // Flip Y for Vulkan NDC

    gl_Position = vec4(clipPos, 0.0, 1.0);
    v_texCoord = in_texCoord;
}

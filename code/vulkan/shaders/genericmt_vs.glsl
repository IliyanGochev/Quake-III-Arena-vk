#version 450 core

// Generic multi-texture vertex shader
// Input: position (binding 0) + texcoord0 (binding 1) + texcoord1 (binding 2) + color (binding 3)

layout(binding = 0) uniform ViewVSBlock
{
    mat4 u_projectionMatrix;
    mat4 u_modelViewMatrix;
    vec2 u_depthRange;
} u_viewVS;

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_texCoord0;
layout(location = 2) in vec2 in_texCoord1;
layout(location = 3) in vec4 in_color;

layout(location = 0) out vec2 v_texCoord0;
layout(location = 1) out vec2 v_texCoord1;
layout(location = 2) out vec4 v_color;

void main()
{
    gl_Position = u_viewVS.u_projectionMatrix * u_viewVS.u_modelViewMatrix * in_position;
    v_texCoord0 = in_texCoord0;
    v_texCoord1 = in_texCoord1;
    v_color = in_color;
}

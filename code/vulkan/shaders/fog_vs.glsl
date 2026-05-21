#version 450 core

// Fog volume rendering vertex shader
// Same vertex input as genericst: position (binding 0) + texcoord (binding 1) + color (binding 2)

layout(binding = 0) uniform ViewVSBlock
{
    mat4 u_projectionMatrix;
    mat4 u_modelViewMatrix;
    vec2 u_depthRange;
} u_viewVS;

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec2 in_texCoord;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 v_texCoord;
layout(location = 1) out vec4 v_color;

void main()
{
    gl_Position = u_viewVS.u_projectionMatrix * u_viewVS.u_modelViewMatrix * in_position;
    v_texCoord = in_texCoord;
    v_color = in_color;
}

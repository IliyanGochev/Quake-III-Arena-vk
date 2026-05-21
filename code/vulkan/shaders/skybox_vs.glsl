#version 450 core

// Skybox vertex shader

layout(binding = 0) uniform ViewVSBlock
{
    mat4 u_projectionMatrix;
    mat4 u_modelViewMatrix;
    vec2 u_depthRange;
} u_viewVS;

layout(binding = 1) uniform SkyBoxVSBlock
{
    vec4 u_eyePos;
} u_skyboxVS;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texCoord;

layout(location = 0) out vec2 v_texCoord;
layout(location = 1) out vec3 v_worldPos;

void main()
{
    vec3 pos = in_position;
    pos = pos * 500.0;

    mat4 viewNoTranslate = u_viewVS.u_modelViewMatrix;
    viewNoTranslate[3] = vec4(0.0, 0.0, 0.0, 1.0);

    gl_Position = (u_viewVS.u_projectionMatrix * viewNoTranslate * vec4(pos, 1.0)).xyww;
    v_texCoord = in_texCoord;
    v_worldPos = pos;
}

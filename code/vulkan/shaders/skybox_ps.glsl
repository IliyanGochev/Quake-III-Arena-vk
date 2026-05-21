#version 450 core

// Skybox fragment shader

layout(binding = 3) uniform sampler2D u_texture;

layout(binding = 2) uniform SkyBoxPSBlock
{
    vec4 u_color;
} u_skyboxPS;

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec3 v_worldPos;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 texColor = texture(u_texture, v_texCoord);
    fragColor = texColor * u_skyboxPS.u_color;
}

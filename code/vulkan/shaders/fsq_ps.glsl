#version 450 core

// Full-screen quad fragment shader for 2D rendering

layout(binding = 3) uniform sampler2D u_texture;

layout(binding = 2) uniform QuadColorBlock
{
    vec4 u_color;
} u_quadColor;

layout(location = 0) in vec2 v_texCoord;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 texColor = texture(u_texture, v_texCoord);
    fragColor = vec4(texColor.rgb * u_quadColor.u_color.rgb, texColor.a);
}

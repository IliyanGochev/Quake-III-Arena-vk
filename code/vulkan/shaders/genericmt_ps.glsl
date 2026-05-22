#version 450 core

// Generic multi-texture fragment shader

layout(binding = 3) uniform sampler2D u_texture0;
layout(binding = 4) uniform sampler2D u_texture1;

layout(location = 0) in vec2 v_texCoord0;
layout(location = 1) in vec2 v_texCoord1;
layout(location = 2) in vec4 v_color;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform ViewPSBlock
{
    vec4 u_clipPlane;
    vec2 u_alphaClip;
} u_viewPS;

void main()
{
    float alpha = texture(u_texture0, v_texCoord0).a;
    if (u_viewPS.u_alphaClip.x > 0.0)
    {
        if (alpha <= u_viewPS.u_alphaClip.y)
            discard;
    }
    else if (u_viewPS.u_alphaClip.x < 0.0)
    {
        if (alpha >= u_viewPS.u_alphaClip.y)
            discard;
    }

    vec4 texColor0 = texture(u_texture0, v_texCoord0);
    vec4 texColor1 = texture(u_texture1, v_texCoord1);
    fragColor = texColor0 * texColor1 * v_color;
}

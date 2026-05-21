#version 450 core

// Generic single-texture fragment shader

layout(binding = 3) uniform sampler2D u_texture;

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform ViewPSBlock
{
    vec4 u_clipPlane;
    vec2 u_alphaClip;
} u_viewPS;

void main()
{
    // Alpha clipping
    float alpha = texture(u_texture, v_texCoord).a;
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

    vec4 texColor = texture(u_texture, v_texCoord);
    fragColor = texColor * v_color;
}

#version 450 core

// Fog volume rendering fragment shader
// Samples fog texture and modulates with vertex color

layout(binding = 3) uniform sampler2D u_texture;

layout(binding = 0) uniform ViewPSBlock
{
    vec4 u_clipPlane;
    vec2 u_alphaClip;
} u_viewPS;

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 fogColor = texture(u_texture, v_texCoord);
    float alpha = fogColor.a;

    // Alpha clipping
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

    fragColor = fogColor * v_color;
}

#version 450

// Fragment inputs
layout(location = 0) in vec2 inTexCoord0;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inViewPos;

// Fragment output
layout(location = 0) out vec4 outColor;

// Uniform buffer - matches psUniformData_t (std140 layout)
layout(std140, set = 0, binding = 1) uniform ViewDataPS {
    vec4 ClipPlane;
    vec2 AlphaClip;  // x: enable/mode (-1, 0, 1), y: threshold
};

// Textures and sampler
layout(set = 0, binding = 2) uniform texture2D Diffuse;
layout(set = 0, binding = 4) uniform sampler Sampler;

void main()
{
    // Sample texture
    vec4 texColor = texture(sampler2D(Diffuse, Sampler), inTexCoord0);

    // Clip plane test (like D3D11's ClipToPlane)
    if (ClipPlane.x != 0.0 || ClipPlane.y != 0.0 || ClipPlane.z != 0.0) {
        float dist = dot(inViewPos, ClipPlane);
        if (dist < 0.0) {
            discard;
        }
    }

    // Modulate with vertex color
    vec4 color = texColor * inColor;

    // Alpha test (like D3D11's FinalColor)
    // AlphaClip.x: mode (-1 = LT, 0 = disabled, 1 = GE/GT)
    // AlphaClip.y: threshold
    float alphaTest = color.a - AlphaClip.y;
    float testValue = AlphaClip.x * alphaTest;
    float enabled = abs(AlphaClip.x);
    if (enabled > 0.5 && testValue < 0.0) {
        discard;
    }

    outColor = color;
}

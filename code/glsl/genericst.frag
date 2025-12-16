#version 450

// Fragment inputs
layout(location = 0) in vec2 inTexCoord0;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 inViewPos;

// Fragment output
layout(location = 0) out vec4 outColor;

// Textures and sampler
layout(set = 0, binding = 2) uniform texture2D Diffuse;
layout(set = 0, binding = 4) uniform sampler Sampler;

void main()
{
    // Sample texture and modulate with vertex color
    vec4 texColor = texture(sampler2D(Diffuse, Sampler), inTexCoord0);
    outColor = texColor * inColor;
}

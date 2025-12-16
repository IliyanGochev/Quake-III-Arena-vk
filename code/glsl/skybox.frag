#version 450

// Fragment inputs
layout(location = 0) in vec2 inTexCoord0;

// Fragment output
layout(location = 0) out vec4 outColor;

// Textures and sampler
layout(set = 0, binding = 2) uniform texture2D Diffuse;
layout(set = 0, binding = 4) uniform sampler Sampler;

void main()
{
    outColor = texture(sampler2D(Diffuse, Sampler), inTexCoord0);
}

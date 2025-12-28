#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;
layout(location = 2) in vec4 inColor;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outViewPos;

// Uniform buffer at binding 0 (matches ViewDataVS layout)
// HLSL cbuffers use column-major by default, same as GLSL std140
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 UboProjection;
    mat4 UboView;
    vec2 UboDepthRange;  // x: DepthRangeMin, y: DepthRange (max - min)
    vec2 _pad0;          // padding for std140 alignment
    vec3 UboEyePos;      // Camera position (unused in generic shader, but layout must match)
};

void main()
{
    // Always use 3D path: full transform through View and Projection
    // (2D rendering uses dedicated image2d shader, not this one)
    vec4 viewPos = UboView * inPosition;
    vec4 clipPos = UboProjection * viewPos;

    // Depth range hack (matches D3D11 DepthRangeHack exactly)
    // OpenGL projection outputs NDC Z in [-1,1], Vulkan expects [0,1]
    float ndcZ = clipPos.z / clipPos.w;
    ndcZ = (ndcZ + 1.0) * 0.5;  // Convert OpenGL [-1,1] to Vulkan [0,1]
    ndcZ = UboDepthRange.x + ndcZ * UboDepthRange.y;  // Apply depth range
    clipPos.z = ndcZ * clipPos.w;  // Un-divide to restore clip space

    gl_Position = clipPos;

    // Pass vertex color - if this shows black, inColor attribute isn't bound correctly
    outColor = inColor;
    outTexCoord0 = inTexCoord0;
    outViewPos = viewPos;
}

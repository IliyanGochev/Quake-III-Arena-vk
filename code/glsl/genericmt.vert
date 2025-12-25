#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inTexCoord1;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;
layout(location = 1) out vec2 outTexCoord1;
layout(location = 2) out vec4 outColor;
layout(location = 3) out vec4 outViewPos;

// Uniform buffer at binding 0 (matches ViewDataVS layout)
// HLSL cbuffers use column-major by default, same as GLSL std140
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 Projection;
    mat4 View;
    vec2 DepthRange;  // x: DepthRangeMin, y: DepthRange (max - min)
    vec2 _pad0;       // padding for std140 alignment
    vec3 EyePos;      // Camera position (unused in generic shader, but layout must match)
};

void main()
{
    // Always use 3D path: full transform through View and Projection
    // (2D rendering uses dedicated image2d shader, not this one)
    vec4 viewPos = View * inPosition;
    vec4 clipPos = Projection * viewPos;

    // Depth range hack (matches D3D11 DepthRangeHack exactly)
    // Apply depth range to clip space Z, let hardware clamp to [0,1]
    float ndcZ = clipPos.z / clipPos.w;
    ndcZ = DepthRange.x + ndcZ * DepthRange.y;  // Apply depth range
    clipPos.z = ndcZ * clipPos.w;  // Un-divide to restore clip space

    gl_Position = clipPos;
    outTexCoord0 = inTexCoord0;
    outTexCoord1 = inTexCoord1;
    outColor = inColor;
    outViewPos = viewPos;
}

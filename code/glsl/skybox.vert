#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;

// Uniform buffer at binding 0 (matches ViewDataVS layout)
// HLSL cbuffers use column-major by default, same as GLSL std140
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 Projection;
    mat4 View;
    vec2 DepthRange;  // x: DepthRangeMin, y: DepthRange (max - min)
    vec2 _pad0;       // padding for std140 alignment
    vec3 EyePos;      // Camera position for skybox centering
};

void main()
{
    // Offset skybox vertices by camera position so skybox always surrounds the viewer
    // This matches the D3D11 skybox shader: input.Position + EyePos
    vec4 worldPos = vec4(inPosition.xyz + EyePos, 1.0);
    vec4 viewPos = View * worldPos;
    vec4 clipPos = Projection * viewPos;

    // Depth range hack (matches D3D11 DepthRangeHack exactly)
    // OpenGL projection outputs NDC Z in [-1,1], Vulkan expects [0,1]
    float ndcZ = clipPos.z / clipPos.w;
    ndcZ = (ndcZ + 1.0) * 0.5;  // Convert OpenGL [-1,1] to Vulkan [0,1]
    ndcZ = DepthRange.x + ndcZ * DepthRange.y;  // Apply depth range
    clipPos.z = ndcZ * clipPos.w;  // Un-divide to restore clip space

    gl_Position = clipPos;
    outTexCoord0 = inTexCoord0;
}

#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;
layout(location = 2) in vec4 inColor;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outViewPos;

// Uniform buffer at binding 0
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 UboProjection;
    mat4 UboView;
    vec2 UboDepthRange;
};

void main()
{
    vec4 clipPos;
    vec4 viewPos;

    // Detect 2D vs 3D by input Z coordinate:
    // 2D screen coords have Z=0, 3D world coords have non-zero Z
    bool is2D = (abs(inPosition.z) < 0.01);

    if (is2D) {
        // 2D mode: skip View matrix, use projection only
        clipPos = UboProjection * inPosition;
        viewPos = inPosition;
        // Flip Y for 2D: OpenGL Y-up -> Vulkan Y-down
        clipPos.y = -clipPos.y;
    } else {
        // 3D mode: full transform
        viewPos = UboView * inPosition;
        clipPos = UboProjection * viewPos;
        // Flip X in clip space to unmirror (position will be off, fix later)
        clipPos.x = -clipPos.x;
    }

    // Force Z to 0.5 for now
    clipPos.z = 0.5;

    gl_Position = clipPos;

    // Pass through vertex color and texture coords
    outColor = inColor;
    outTexCoord0 = inTexCoord0;
    outViewPos = viewPos;
}

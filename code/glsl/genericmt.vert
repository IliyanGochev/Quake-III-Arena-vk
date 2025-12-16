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

// Uniform buffer - matches vsUniformData_t (std140 layout)
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 Projection;
    mat4 View;
    vec2 DepthRange;  // x: min, y: range (max - min)
};

void main()
{
    vec4 clipPos;
    vec4 viewPos;

    // Detect 2D vs 3D by input Z coordinate
    bool is2D = (abs(inPosition.z) < 0.01);

    if (is2D) {
        // 2D mode: skip View matrix
        clipPos = Projection * inPosition;
        viewPos = inPosition;
    } else {
        // 3D mode: full transform
        viewPos = View * inPosition;
        clipPos = Projection * viewPos;
    }

    // Flip Y: OpenGL Y-up -> Vulkan Y-down
    clipPos.y = -clipPos.y;

    // Force Z to 0.5 for now
    clipPos.z = 0.5;

    gl_Position = clipPos;
    outTexCoord0 = inTexCoord0;
    outTexCoord1 = inTexCoord1;
    outColor = inColor;
    outViewPos = viewPos;
}

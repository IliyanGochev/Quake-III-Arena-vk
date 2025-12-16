#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;

// Uniform buffers (std140 layout)
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 Projection;
    mat4 View;
    vec2 DepthRange;
};

// Skybox-specific data - using push constants or second UBO
// For now, we'll handle EyePos differently - it should be baked into View matrix
// or passed via push constant. Using View matrix approach for simplicity.

void main()
{
    vec4 clipPos;

    // Detect 2D vs 3D by input Z coordinate
    bool is2D = (abs(inPosition.z) < 0.01);

    if (is2D) {
        // 2D mode: skip View matrix
        clipPos = Projection * inPosition;
    } else {
        // 3D mode: full transform
        vec4 viewPos = View * inPosition;
        clipPos = Projection * viewPos;
    }

    // Flip Y: OpenGL Y-up -> Vulkan Y-down
    clipPos.y = -clipPos.y;

    // Force Z to 0.5 for now
    clipPos.z = 0.5;

    gl_Position = clipPos;
    outTexCoord0 = inTexCoord0;
}

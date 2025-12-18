#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;

// Uniform buffer at binding 0 (matches HLSL ViewDataVS)
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 Projection;
    mat4 View;
    vec2 DepthRange;  // x: DepthRangeMin, y: DepthRange (max - min)
};

void main()
{
    vec4 viewPos;
    vec4 clipPos;

    // Detect 2D vs 3D: 2D screen-space quads have Z near 0
    bool is2D = (abs(inPosition.z) < 0.01);

    if (is2D) {
        // 2D mode: vertices are in screen space, skip View matrix
        clipPos = Projection * inPosition;
        viewPos = inPosition;

        // Flip Y for Vulkan - DISABLED: using negative viewport height instead
        //clipPos.y = -clipPos.y;

        // 2D is drawn after 3D - put at front so it passes depth test and appears on top
        clipPos.z = 0.0;
    } else {
        // 3D mode: full transform through View and Projection
        viewPos = View * inPosition;
        clipPos = Projection * viewPos;

        // Flip Y: OpenGL Y-up -> Vulkan Y-down - DISABLED: using negative viewport height instead
        //clipPos.y = -clipPos.y;

        // Simple depth conversion: OpenGL NDC z in [-1,1] -> Vulkan [0,1]
        clipPos.z = (clipPos.z * 0.5 + 0.5 * clipPos.w);
    }

    gl_Position = clipPos;
    outTexCoord0 = inTexCoord0;
}

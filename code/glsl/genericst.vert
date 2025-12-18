#version 450

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord0;
layout(location = 2) in vec4 inColor;

// Vertex outputs
layout(location = 0) out vec2 outTexCoord0;
layout(location = 1) out vec4 outColor;
layout(location = 2) out vec4 outViewPos;

// Uniform buffer at binding 0 (matches HLSL ViewDataVS)
layout(std140, set = 0, binding = 0) uniform ViewDataVS {
    mat4 UboProjection;
    mat4 UboView;
    vec2 UboDepthRange;  // x: DepthRangeMin, y: DepthRange (max - min)
};

void main()
{
    vec4 viewPos;
    vec4 clipPos;

    // Detect 2D vs 3D: 2D screen-space quads have Z near 0
    bool is2D = (abs(inPosition.z) < 0.01);

    if (is2D) {
        // 2D mode: vertices are in screen space, skip View matrix
        clipPos = UboProjection * inPosition;
        viewPos = inPosition;

        // Flip Y for Vulkan
        //clipPos.y = -clipPos.y;

        // 2D is drawn after 3D - put at front so it passes depth test and appears on top
        clipPos.z = 0.0;
    } else {
        // 3D mode: full transform through View and Projection
        viewPos = UboView * inPosition;
        clipPos = UboProjection * viewPos;

        // Flip Y: OpenGL Y-up -> Vulkan Y-down
        //clipPos.y = -clipPos.y;

        // Simple depth conversion: OpenGL NDC z in [-1,1] -> Vulkan [0,1]
        // Skip the depth range hack for now to isolate the issue
        clipPos.z = (clipPos.z * 0.5 + 0.5 * clipPos.w);
    }

    gl_Position = clipPos;

    // Pass through vertex color and texture coords
    outColor = inColor;
    outTexCoord0 = inTexCoord0;
    outViewPos = viewPos;
}

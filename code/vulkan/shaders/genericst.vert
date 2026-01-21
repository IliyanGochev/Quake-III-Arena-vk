#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Generic single-texture vertex shader
//----------------------------------------------------------------------------

// Vertex inputs
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

// Uniform buffer - View constants (vertex shader)
layout(set = 0, binding = 0) uniform ViewVS {
    mat4 projection;
    mat4 modelView;
    vec2 depthRange;  // Legacy, not needed in Vulkan 1.1+
} viewVS;

// Outputs to fragment shader
layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec4 fragViewPos;

void main() {
    // Transform to view space
    fragViewPos = viewVS.modelView * inPosition;

    // Transform to clip space
    gl_Position = viewVS.projection * fragViewPos;

    // Pass through texture coordinates and color
    fragTexCoord = inTexCoord;
    fragColor = inColor;
}

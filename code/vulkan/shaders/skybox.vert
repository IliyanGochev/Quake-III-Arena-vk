#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Skybox vertex shader
//----------------------------------------------------------------------------

// Vertex inputs
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;

// Uniform buffer - View constants (vertex shader)
layout(set = 0, binding = 0) uniform ViewVS {
    mat4 projection;
    mat4 modelView;
    vec2 depthRange;  // Legacy, not needed in Vulkan 1.1+
} viewVS;

// Push constant for eye position offset
layout(push_constant) uniform PushConstants {
    vec3 eyePos;
} push;

// Outputs to fragment shader
layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Offset skybox position by eye position
    vec4 pos = vec4(inPosition + push.eyePos, 1.0);

    // Transform to view space
    vec4 viewPos = viewVS.modelView * pos;

    // Transform to clip space
    gl_Position = viewVS.projection * viewPos;

    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
}

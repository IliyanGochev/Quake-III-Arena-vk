#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Fullscreen quad / 2D vertex shader (for UI, cinematics, etc.)
//----------------------------------------------------------------------------

// Vertex inputs
layout(location = 0) in vec2 inPosition;  // 2D position
layout(location = 1) in vec2 inTexCoord;

// Uniform buffer - View constants (vertex shader)
// For 2D rendering, this typically contains an orthographic projection
layout(set = 0, binding = 0) uniform ViewVS {
    mat4 projection;
    mat4 modelView;
    vec2 depthRange;  // Legacy, not needed in Vulkan 1.1+
} viewVS;

// Outputs to fragment shader
layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Match D3D11: Apply modelView then projection
    vec4 viewPos = viewVS.modelView * vec4(inPosition, 0.0, 1.0);
    vec4 sPos = viewVS.projection * viewPos;

    // Match D3D11: Override Z and W to ensure 2D elements aren't clipped
    gl_Position = vec4(sPos.xy, 0.0, 1.0);

    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
}

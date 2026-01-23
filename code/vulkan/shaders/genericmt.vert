#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Generic multi-texture vertex shader (diffuse + lightmap)
//----------------------------------------------------------------------------

// Vertex inputs
layout(location = 0) in vec3 inPosition;  // Changed from vec4 to vec3
layout(location = 1) in vec2 inTexCoord0;  // Diffuse texture
layout(location = 2) in vec2 inTexCoord1;  // Lightmap texture
layout(location = 3) in vec4 inColor;

// Uniform buffer - View constants (vertex shader)
layout(set = 0, binding = 0) uniform ViewVS {
    mat4 projection;
    mat4 modelView;
    vec2 depthRange;  // Legacy, not needed in Vulkan 1.1+
} viewVS;

// Outputs to fragment shader
layout(location = 0) out vec2 fragTexCoord0;
layout(location = 1) out vec2 fragTexCoord1;
layout(location = 2) out vec4 fragColor;
layout(location = 3) out vec4 fragViewPos;

void main() {
    // Transform to view space - explicitly set w=1.0 like D3D11 does
    fragViewPos = viewVS.modelView * vec4(inPosition, 1.0);

    // Transform to clip space
    gl_Position = viewVS.projection * fragViewPos;

    // Pass through texture coordinates and color
    fragTexCoord0 = inTexCoord0;
    fragTexCoord1 = inTexCoord1;
    fragColor = inColor;
}

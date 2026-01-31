#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Fullscreen quad / 2D fragment shader (for UI, cinematics, etc.)
//----------------------------------------------------------------------------

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord;

// Uniform buffer - View constants (pixel shader) - for alpha testing
layout(set = 0, binding = 1) uniform ViewPS {
    vec4 clipPlane;      // Clip plane for portals (xyz = normal, w = distance)
    vec2 alphaTest;      // x = sign, y = threshold
} viewPS;

// Uniform buffer - Stage data (for color modulation)
layout(set = 1, binding = 0) uniform StageData {
    vec4 color;  // Color modulation
} stage;

// Textures
layout(set = 2, binding = 0) uniform sampler2D tex;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec4 texColor = texture(tex, fragTexCoord);

    // Apply color modulation
    vec4 finalColor = texColor * stage.color;

    // Alpha test (matches D3D11 FinalColor function in pscommon.h)
    // clip((c.a - c_AlphaClip[1]) * c_AlphaClip[0]);
    if ((finalColor.a - viewPS.alphaTest.y) * viewPS.alphaTest.x < 0.0) {
        discard;
    }

    outColor = finalColor;
}

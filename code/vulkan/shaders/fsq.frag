#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Fullscreen quad / 2D fragment shader (for UI, cinematics, etc.)
//----------------------------------------------------------------------------

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord;

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
    outColor = texColor * stage.color;
}

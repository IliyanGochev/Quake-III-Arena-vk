#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Skybox fragment shader
//----------------------------------------------------------------------------

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord;

// Uniform buffer - Stage data (for color tint)
layout(set = 1, binding = 0) uniform StageData {
    vec4 color;  // Color tint for fog/atmosphere effects
} stage;

// Textures
layout(set = 2, binding = 0) uniform sampler2D skyboxTex;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Sample skybox texture
    vec4 texColor = texture(skyboxTex, fragTexCoord);

    // Apply color tint (for atmospheric effects)
    outColor = texColor * stage.color;
}

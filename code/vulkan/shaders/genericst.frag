#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Generic single-texture fragment shader
//----------------------------------------------------------------------------

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec4 fragViewPos;

// Uniform buffer - View constants (pixel shader)
layout(set = 0, binding = 1) uniform ViewPS {
    vec4 clipPlane;      // Clip plane for portals (xyz = normal, w = distance)
    vec2 alphaTest;      // x = sign, y = threshold
} viewPS;

// Textures
layout(set = 2, binding = 0) uniform sampler2D diffuseTex;

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Clip plane test (for portals)
    if (viewPS.clipPlane.w != 0.0) {
        float dist = dot(viewPS.clipPlane.xyz, fragViewPos.xyz) - viewPS.clipPlane.w;
        if (dist < 0.0) {
            discard;
        }
    }

    // Sample texture
    vec4 texColor = texture(diffuseTex, fragTexCoord);

    // Modulate with vertex color
    vec4 color = texColor * fragColor;

    // Alpha test (emulated for GLS_ATEST_* modes)
    if (viewPS.alphaTest.x != 0.0) {
        float alphaRef = viewPS.alphaTest.y;
        if ((color.a - alphaRef) * viewPS.alphaTest.x < 0.0) {
            discard;
        }
    }

    outColor = color;
}

#version 450
#extension GL_ARB_separate_shader_objects : enable

//----------------------------------------------------------------------------
// Generic multi-texture fragment shader (diffuse * lightmap)
//----------------------------------------------------------------------------

// Inputs from vertex shader
layout(location = 0) in vec2 fragTexCoord0;  // Diffuse
layout(location = 1) in vec2 fragTexCoord1;  // Lightmap
layout(location = 2) in vec4 fragColor;
layout(location = 3) in vec4 fragViewPos;

// Uniform buffer - View constants (pixel shader)
layout(set = 0, binding = 1) uniform ViewPS {
    vec4 clipPlane;      // Clip plane for portals (xyz = normal, w = distance)
    vec2 alphaTest;      // x = sign, y = threshold
} viewPS;

// Textures
layout(set = 2, binding = 0) uniform sampler2D diffuseTex;
layout(set = 2, binding = 1) uniform sampler2D lightmapTex;

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

    // Sample textures
    vec4 diffuse = texture(diffuseTex, fragTexCoord0);
    vec4 lightmap = texture(lightmapTex, fragTexCoord1);

    // Multiply diffuse and lightmap (standard Q3 lightmapping)
    vec4 color = diffuse * lightmap * fragColor;

    // Alpha test (emulated for GLS_ATEST_* modes)
    if (viewPS.alphaTest.x != 0.0) {
        float alphaRef = viewPS.alphaTest.y;
        if ((color.a - alphaRef) * viewPS.alphaTest.x < 0.0) {
            discard;
        }
    }

    outColor = color;
}

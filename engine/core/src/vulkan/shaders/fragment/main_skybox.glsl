#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
layout(location = 0) in vec2 inTexCoord;
#endif
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inViewPos; // Just pass z?
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inLightSpacePos[NUM_SHADOW_MAPS];
#ifdef FEATURE_NORMAL_MAPPING
layout(location = 7) in vec3 inTangent;
layout(location = 8) in vec3 inBitangent;
#endif

#ifdef FEATURE_LIGHTING
#include "fragment/lighting.glsl"
#endif
#ifdef FEATURE_MATERIALS
#include "fragment/materials.glsl"
#endif

#ifndef RENDER_PASS_SHADOW
layout(location = 0) out vec4 outColour;
#endif

void main()
{
  vec3 texel = texture(cubeMapSampler, inWorldPos).rgb;
  outColour = vec4(texel, 1.0);
}

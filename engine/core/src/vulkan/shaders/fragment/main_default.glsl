#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
layout(location = 0) in vec2 inTexCoord;
#endif
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inLightSpacePos;
#ifdef FEATURE_NORMAL_MAPPING
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;
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

layout(push_constant) uniform PushConstants
{
  layout(offset = 64) vec4 colour;
} constants;

void main()
{
#ifdef FEATURE_NORMAL_MAPPING
  vec3 tangentSpaceNormal = normalize(texture(normalMapSampler[0], inTexCoord).rgb * 2.0 - 1.0);
  mat3 tbn = mat3(inTangent, inBitangent, inNormal);
  vec3 normal = normalize(tbn * tangentSpaceNormal);
#else
  vec3 normal = normalize(inNormal);
#endif

#ifdef FEATURE_LIGHTING
  vec3 light = computeLight(inWorldPos, normal);
#else
  vec3 light = vec3(1, 1, 1);
#endif

#ifdef FEATURE_TEXTURE_MAPPING
  vec4 texel = computeTexel(inTexCoord);
#else
  vec4 texel = material.colour;
#endif

//  if (texel.a < 0.5) {
//    discard;
//  }
//  else {
    outColour = constants.colour * vec4(light, 1) * texel;
//  }
}

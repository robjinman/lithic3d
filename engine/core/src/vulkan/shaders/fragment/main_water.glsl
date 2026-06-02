#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"
#include "fragment/effects.glsl"

layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec3 inViewPos; // Just pass z?
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inLightSpacePos[NUM_SHADOW_MAPS];

#ifdef FEATURE_LIGHTING
#include "fragment/lighting.glsl"
#endif

layout(location = 0) out vec4 outColour;

#ifdef FEATURE_MATERIALS
#include "fragment/materials.glsl"
#endif

layout(push_constant) uniform PushConstants
{
  layout(offset = 80) vec4 colour;
  uint tick;
} constants;

void main()
{
  vec3 normal = normalize(inNormal);

#ifdef FEATURE_LIGHTING
  vec3 light = computeLight(inWorldPos, normal, false);
#else
  vec3 light = vec3(1, 1, 1);
#endif

  float shimmerMin = 0.6;
  float shimmerMax = 1.0;
  float shimmerNorm = (1.0 + sin(inWorldPos.x * 0.1 + constants.tick * 0.04)) * 0.5;
  float shimmerMag = shimmerMin + (shimmerMax - shimmerMin) * shimmerNorm;
  vec4 shimmer = vec4(shimmerMag, shimmerMag, shimmerMag, 1);

//  if (texel.a < 0.5) {
//    discard;
//  }
//  else {
  outColour = applyFog(vec4(light, 1) * material.colour * shimmer, -inViewPos.z);
//  }
}

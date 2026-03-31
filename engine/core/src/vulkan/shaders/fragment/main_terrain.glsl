#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"
#include "fragment/effects.glsl"

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

layout(push_constant) uniform PushConstants
{
  layout(offset = 80) vec4 colour;
} constants;

vec4 computeTexelFromSplats(vec2 texCoord)
{
  // Number of tiles to 1 cell length
  // TODO: Don't hard-code
  float numTiles = 250.0;

  vec4 splat = texture(texSampler[0], texCoord);

  vec4 texel = splat[0] * texture(texSampler[1], texCoord * numTiles)
    + splat[1] * texture(texSampler[2], texCoord * numTiles)
    + splat[2] * texture(texSampler[3], texCoord * numTiles)
    + (1.0 - splat[3]) * texture(texSampler[4], texCoord * numTiles);

  return vec4(texel.xyz, 1);
}

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
  vec4 texel = computeTexelFromSplats(inTexCoord);
#else
  vec4 texel = material.colour;
#endif

//  if (texel.a < 0.5) {
//    discard;
//  }
//  else {
    outColour = applyFog(constants.colour * vec4(light, 1) * texel, -inViewPos.z);
//  }
}

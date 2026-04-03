#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"
#include "fragment/materials.glsl"

layout(location = 1) in vec3 inWorldPos;

#ifndef RENDER_PASS_SHADOW
layout(location = 0) out vec4 outColour;
#endif

vec4 applyFogSky(vec4 fragColour)
{
  const float fogHeight = DRAW_DISTANCE;
  float y = inWorldPos.y * DRAW_DISTANCE / length(inWorldPos);
  float t = min(1, max(fogHeight - y, 0) / fogHeight);

  return vec4(fragColour.xyz + t * (vec3(1) - fragColour.xyz), fragColour.w);
}

void main()
{
  vec3 texel = texture(cubeMapSampler, inWorldPos).rgb;
  outColour = applyFogSky(vec4(texel, 1.0));
}

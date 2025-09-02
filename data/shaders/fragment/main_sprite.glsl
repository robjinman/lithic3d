#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inWorldPos;
layout(location = 6) in vec4 inColour;

#include "fragment/materials.glsl"

layout(location = 0) out vec4 outColour;

void main()
{
  vec4 texel = computeTexel(inTexCoord);

//  if (texel.a < 0.5) {
//    discard;
//  }
//  else {
    outColour = inColour * texel;
//  }
}

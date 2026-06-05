#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(location = 0) in vec4 inColour;
layout(location = 1) in vec2 inTexCoord;

#include "fragment/materials.glsl"

layout(location = 0) out vec4 outColour;

void main()
{
  vec4 texel = computeTexel(inTexCoord);

  outColour = inColour * texel;
}

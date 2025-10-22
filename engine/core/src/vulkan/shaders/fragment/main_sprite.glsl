#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(location = 0) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants
{
  layout(offset = 96) vec4 colour;
} constants;

#include "fragment/materials.glsl"

layout(location = 0) out vec4 outColour;

void main()
{
  vec4 texel = computeTexel(inTexCoord);
  outColour = constants.colour * texel;
}

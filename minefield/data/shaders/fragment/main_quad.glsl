#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(push_constant) uniform PushConstants
{
  layout(offset = 64) vec4 colour;
} constants;

layout(location = 0) out vec4 outColour;

void main()
{
  outColour = constants.colour;
}

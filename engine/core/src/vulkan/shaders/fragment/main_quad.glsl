#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "common.glsl"

layout(location = 0) in vec2 inUv;

layout(push_constant) uniform PushConstants
{
  layout(offset = 64) vec4 colour;
  float radius;
} constants;

layout(location = 0) out vec4 outColour;

bool isBetween(float x, float a, float b)
{
  return a < b ? x >= a && x <= b : x <= a && x >= b;
}

bool insideRect(vec2 p, vec2 A, vec2 B)
{
  return isBetween(p.x, A.x, B.x) && isBetween(p.y, A.y, B.y);
}

bool insideCircle(vec2 p, vec2 centre, float sqRadius)
{
  return dot(p - centre, p - centre) < sqRadius;
}

void main()
{
  if (constants.radius != 0.0) {
    const float r = constants.radius;
    const float rSq = r * r;

    vec2 C00 = vec2(r, r);
    vec2 C10 = vec2(1.0 - r, r);
    vec2 C11 = vec2(1.0 - r, 1.0 - r);
    vec2 C01 = vec2(r, 1.0 - r);

    if (!insideRect(inUv, C00, C11)) {
      if (insideRect(inUv, vec2(0.0, 0.0), C00) && !insideCircle(inUv, C00, rSq)) {
        discard;
      }

      if (insideRect(inUv, vec2(1.0, 0.0), C10) && !insideCircle(inUv, C10, rSq)) {
        discard;
      }

      if (insideRect(inUv, vec2(1.0, 1.0), C11) && !insideCircle(inUv, C11, rSq)) {
        discard;
      }

      if (insideRect(inUv, vec2(0.0, 1.0), C01) && !insideCircle(inUv, C01, rSq)) {
        discard;
      }
    }
  }

  outColour = constants.colour;
}

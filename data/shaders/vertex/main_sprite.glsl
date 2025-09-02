#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_GLOBAL, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 6) out vec4 outColour;

void main()
{
  mat3 modelMatrix = mat3(inTransform0, inTransform1, inTransform2);

  vec3 worldPos3 = modelMatrix * inPos;
  vec4 worldPos = vec4(worldPos3, 1.f);

  gl_Position = camera.projMatrix * camera.viewMatrix * worldPos;

  outWorldPos = worldPos3;

  int i = gl_InstanceIndex;
  float c = (i % 10) / 9.0;

  outColour = vec4(c, 1.0 - c, 1.0, 1.0);// inColour;
  outTexCoord = inUvRect.xy + inTexCoord * inUvRect.zw;
}

#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_GLOBAL, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
  vec2 uvCoords[4];
} constants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;

vec4 computeVertexPosition(mat4 modelMatrix)
{
  return modelMatrix * vec4(inPos, 1.0);
}

void main()
{
  mat4 modelMatrix = constants.modelMatrix;

  vec4 worldPos = computeVertexPosition(modelMatrix);
  gl_Position = camera.projMatrix * camera.viewMatrix * worldPos;

  outWorldPos = worldPos.xyz;
  outTexCoord = constants.uvCoords[gl_VertexIndex];
  outNormal = mat3(modelMatrix) * inNormal;
}

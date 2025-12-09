#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_RENDER_PASS, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
} constants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;

vec4 computeVertexPosition(mat4 modelMatrix)
{
  return modelMatrix * vec4(inPos, 1.0);
}

void main()
{
  mat4 modelMatrix = constants.modelMatrix;

  vec4 worldPos = computeVertexPosition(modelMatrix);
  gl_Position = camera.projMatrix * camera.viewMatrix * worldPos;

  switch (gl_VertexIndex) {
    case 0: outTexCoord = vec2(0.0, 0.0); break;
    case 1: outTexCoord = vec2(1.0, 0.0); break;
    case 2: outTexCoord = vec2(1.0, 1.0); break;
    case 3: outTexCoord = vec2(0.0, 1.0); break;
  }
  outWorldPos = worldPos.xyz;
}

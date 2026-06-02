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

layout(location = 0) out vec4 outColour;

void main()
{
  mat4 modelMatrix = constants.modelMatrix;
  vec4 worldPos = modelMatrix * vec4(inPos, 1.0);
  vec4 viewPos = camera.viewMatrix * worldPos;
  gl_Position = camera.projMatrix * viewPos;
  gl_PointSize = 5.0;

  outColour = vec4(1, 0, 0, 1); // TODO
}

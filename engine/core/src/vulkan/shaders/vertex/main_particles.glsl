#version 450

#include "common.glsl"

layout(location = 0) in vec3 inVertPos;
layout(location = 1) in vec2 inVertTexCoord;
layout(location = 2) in vec3 inParticlePos;
layout(location = 3) in float inParticleSize;
layout(location = 4) in vec4 inParticleColour;

layout(std140, set = DESCRIPTOR_SET_RENDER_PASS, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;   // Encodes emitter position
} constants;

layout(location = 0) out vec4 outColour;
layout(location = 1) out vec2 outTexCoord;

void main()
{
  mat4 modelMatrix = constants.modelMatrix;
  vec4 worldPos = modelMatrix * vec4(inParticlePos, 1.0);
  vec4 viewPos = camera.viewMatrix * worldPos + vec4(inVertPos * inParticleSize, 0.0);
  gl_Position = camera.projMatrix * viewPos;

  outColour = inParticleColour;
  outTexCoord = inVertTexCoord;
}

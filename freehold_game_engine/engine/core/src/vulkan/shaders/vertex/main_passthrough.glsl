#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_GLOBAL, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(std140, set = DESCRIPTOR_SET_GLOBAL, binding = 1) uniform LightTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} light;

#ifdef FEATURE_VERTEX_SKINNING
#define MAX_JOINTS 128

layout(std140, set = DESCRIPTOR_SET_OBJECT, binding = 0) uniform JointTransformsUbo
{
  mat4 transforms[MAX_JOINTS];
} joints;
#endif

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
} constants;

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
layout(location = 0) out vec2 outTexCoord;
#endif
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec4 outLightSpacePos;
#ifdef FEATURE_NORMAL_MAPPING
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;
#endif

void main()
{
  vec4 worldPos = vec4(inPos, 1.0);
  gl_Position = camera.projMatrix * mat4(mat3(camera.viewMatrix)) * worldPos;
  outWorldPos = worldPos.xyz;
}

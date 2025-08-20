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

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
  vec2 uvCoords[4];
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

vec4 computeVertexPosition(mat4 modelMatrix)
{
  return modelMatrix * vec4(inPos, 1.0);
}

void main()
{
#ifdef ATTR_MODEL_MATRIX
  mat4 modelMatrix = mat4(inModelMatrix0, inModelMatrix1, inModelMatrix2, inModelMatrix3);
#else
  mat4 modelMatrix = constants.modelMatrix;
#endif

  vec4 worldPos = computeVertexPosition(modelMatrix);
#ifdef RENDER_PASS_SHADOW
  gl_Position = light.projMatrix * light.viewMatrix * worldPos;
#else
  gl_Position = camera.projMatrix * camera.viewMatrix * worldPos;
#endif

  outWorldPos = worldPos.xyz;
  outLightSpacePos = light.projMatrix * light.viewMatrix * worldPos;

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
  outTexCoord = constants.uvCoords[gl_VertexIndex];
#endif

#ifdef FEATURE_NORMAL_MAPPING
  vec3 T = normalize(mat3(modelMatrix) * normalize(inTangent));
  vec3 N = normalize(mat3(modelMatrix) * normalize(inNormal));
  T = normalize(T - dot(T, N) * N);
  vec3 B = normalize(cross(N, T));
  outTangent = T;
  outBitangent = B;
  outNormal = N;
#else
  outNormal = mat3(modelMatrix) * inNormal;
#endif
}

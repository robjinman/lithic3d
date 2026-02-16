#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_GLOBAL, binding = 0) uniform LightTransformsUbo
{
  mat4 viewMatrix[NUM_SHADOW_MAPS];
  mat4 projMatrix[NUM_SHADOW_MAPS];
} light;

layout(std140, set = DESCRIPTOR_SET_RENDER_PASS, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

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
  uint shadowCascadeIndex;
} constants;

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
layout(location = 0) out vec2 outTexCoord;
#endif
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outViewPos;
layout(location = 3) out vec3 outNormal;
layout(location = 4) out vec4 outLightSpacePos[NUM_SHADOW_MAPS];
#ifdef FEATURE_NORMAL_MAPPING
layout(location = 7) out vec3 outTangent;
layout(location = 8) out vec3 outBitangent;
#endif

vec4 computeVertexPosition(mat4 modelMatrix)
{
#ifdef FEATURE_VERTEX_SKINNING
  mat4 transform =
    inWeights[0] * joints.transforms[inJoints[0]] +
    inWeights[1] * joints.transforms[inJoints[1]] +
    inWeights[2] * joints.transforms[inJoints[2]] +
    inWeights[3] * joints.transforms[inJoints[3]];

  return modelMatrix * transform * vec4(inPos, 1.0);
#else
  return modelMatrix * vec4(inPos, 1.0);
#endif
}

void main()
{
#ifdef ATTR_MODEL_MATRIX
  mat4 modelMatrix = mat4(inModelMatrix0, inModelMatrix1, inModelMatrix2, inModelMatrix3);
#else
  mat4 modelMatrix = constants.modelMatrix;
#endif

  vec4 worldPos = computeVertexPosition(modelMatrix);

  // TODO: Separate shadow pass shader?
#ifdef RENDER_PASS_SHADOW
  gl_Position = light.projMatrix[constants.shadowCascadeIndex] *
    light.viewMatrix[constants.shadowCascadeIndex] * worldPos;
#else
  vec4 viewPos = camera.viewMatrix * worldPos;
  gl_Position = camera.projMatrix * viewPos;

  outWorldPos = worldPos.xyz;
  outViewPos = viewPos.xyz;

  for (int i = 0; i < NUM_SHADOW_MAPS; ++i) {
    outLightSpacePos[i] = light.projMatrix[i] * light.viewMatrix[i] * worldPos;
  }
#endif

#if defined(FEATURE_TEXTURE_MAPPING) || defined(FEATURE_NORMAL_MAPPING)
  outTexCoord = inTexCoord;
#endif

#ifdef FEATURE_NORMAL_MAPPING
  vec3 T = normalize(mat3(modelMatrix) * normalize(inTangent));
  vec3 N = normalize(mat3(modelMatrix) * normalize(inNormal));
  T = normalize(T - dot(T, N) * N);
  vec3 B = normalize(cross(N, T));
  outTangent = T;
  outBitangent = B;
  outNormal = N;
#elif defined(ATTR_NORMAL)
  outNormal = mat3(modelMatrix) * inNormal;
#else
  outNormal = vec3(0, 1, 0);
#endif
}

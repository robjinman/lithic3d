#define MAX_POINT_LIGHTS 4

struct Light
{
  vec3 worldPos;
  vec3 colour;
  float ambient;
  float specular;
};

layout(std140, set = DESCRIPTOR_SET_RENDER_PASS, binding = 1) uniform LightingUbo
{
  vec3 viewPos;
  int numPointLights;
  Light pointLights[MAX_POINT_LIGHTS];
  Light directionalLight;
} lighting;

layout(set = DESCRIPTOR_SET_RENDER_PASS, binding = 2) uniform sampler2D shadowMapSampler;

float sampleShadowMap(vec2 uv, float lightSpacePosZ)
{
  ivec2 shadowMapSize = textureSize(shadowMapSampler, 0);
  float scale = 0.5;
	float dx = scale / float(shadowMapSize.x);
	float dy = scale / float(shadowMapSize.y);
  int w = 2;

  int shadow = 0;

  for (int i = -w; i <= w; ++i) {
    for (int j = -w; j <= w; ++j) {
      if (lightSpacePosZ > texture(shadowMapSampler, uv + vec2(i * dx, j * dy)).r) {
        ++shadow;
      }
    }
  }

  int W = w * 2 + 1;
  return float(shadow) / (W * W);
}

vec3 computeDirectionalLight(vec3 worldPos, vec3 normal)
{
  // TODO: Do perspective divide in vertex shader?
  vec3 lightSpacePos = (inLightSpacePos / inLightSpacePos.w).xyz;
  vec2 lightSpaceXy = lightSpacePos.xy * 0.5 + 0.5;
  float shadow = sampleShadowMap(lightSpaceXy, lightSpacePos.z);

  float intensity = lighting.directionalLight.ambient;

  vec3 lightDir = normalize(lighting.directionalLight.worldPos - worldPos);
  float diffuse = max(dot(normal, lightDir), 0.0);
  vec3 viewDir = normalize(lighting.viewPos - worldPos);
  vec3 reflectDir = reflect(-lightDir, normal);
  float specular = lighting.directionalLight.specular * pow(max(dot(viewDir, reflectDir), 0.0), 32);
  intensity += (1.0 - shadow) * (diffuse + specular);

  return intensity * lighting.directionalLight.colour;
}

vec3 computePointLight(vec3 worldPos, vec3 normal, int i)
{
  float intensity = lighting.pointLights[i].ambient;

  vec3 lightDir = normalize(lighting.pointLights[i].worldPos - worldPos);
  float diffuse = max(dot(normal, lightDir), 0.0);
  vec3 viewDir = normalize(lighting.viewPos - worldPos);
  vec3 reflectDir = reflect(-lightDir, normal);
  float specular = lighting.pointLights[i].specular * pow(max(dot(viewDir, reflectDir), 0.0), 32);
  intensity += diffuse + specular;

  return intensity * lighting.pointLights[i].colour;
}

vec3 computeLight(vec3 worldPos, vec3 normal)
{
  vec3 light = computeDirectionalLight(worldPos, normal);

  for (int i = 0; i < lighting.numPointLights; ++i) {
    light += computePointLight(worldPos, normal, i);
  }

  return light;
}

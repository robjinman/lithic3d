#define MAX_LIGHTS 8

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
  int numLights;
  Light lights[MAX_LIGHTS];
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

vec3 computeLight(vec3 worldPos, vec3 normal)
{
  vec3 light = vec3(0, 0, 0);

  for (int i = 0; i < lighting.numLights; ++i) {
    float shadow = 0.0;

    // TODO: Currently, only the first light casts shadows
    if (i == 0) {
      vec3 lightSpacePos = (inLightSpacePos / inLightSpacePos.w).xyz;
      vec2 lightSpaceXy = lightSpacePos.xy * 0.5 + 0.5;
      shadow = sampleShadowMap(lightSpaceXy, lightSpacePos.z);
    }

    float intensity = lighting.lights[i].ambient;

    vec3 lightDir = normalize(lighting.lights[i].worldPos - worldPos);
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 viewDir = normalize(lighting.viewPos - worldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = lighting.lights[i].specular * pow(max(dot(viewDir, reflectDir), 0.0), 32);
    intensity += (1.0 - shadow) * (diffuse + specular);

    light += intensity * lighting.lights[i].colour;
  }

  return light;
}

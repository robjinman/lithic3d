layout(std140, set = DESCRIPTOR_SET_MATERIAL, binding = 0) uniform MaterialUbo
{
  vec4 colour;
  // TODO: PBR values
} material;

// TODO: Dynamic size array
layout(set = DESCRIPTOR_SET_MATERIAL, binding = 1) uniform sampler2D texSampler[5];
layout(set = DESCRIPTOR_SET_MATERIAL, binding = 2) uniform sampler2D normalMapSampler[5];
layout(set = DESCRIPTOR_SET_MATERIAL, binding = 3) uniform samplerCube cubeMapSampler;
layout(set = DESCRIPTOR_SET_MATERIAL, binding = 4) uniform sampler2D splatMapSampler;

#if defined(FEATURE_TEXTURE_MAPPING)
vec4 computeTexel(vec2 texCoord)
{
  return material.colour * texture(texSampler[0], texCoord);
}
#endif

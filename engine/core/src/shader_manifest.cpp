#include "lithic3d/shader_manifest.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/strings.hpp"
#include "lithic3d/logger.hpp"
#include <set>
#include <cassert>

namespace fs = std::filesystem;

using namespace lithic3d::render;

namespace lithic3d
{
namespace
{

bool hasAttribute(const VertexLayout& layout, BufferUsage attribute)
{
  return std::find(layout.begin(), layout.end(), attribute) != layout.end();
}

bool isValid(const ShaderProgramSpec& spec, std::string& message)
{
  if (spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {
    if (!hasAttribute(spec.meshFeatures.vertexLayout, BufferUsage::AttrTangent)) {
      message = "Material has normal map, but mesh is missing tangents";
      return false;
    }
    if (!spec.meshFeatures.flags.test(MeshFeatures::HasTangents)) {
      message = "Material has normal map, but mesh is missing tangents";
      return false;
    }
    if (!hasAttribute(spec.meshFeatures.vertexLayout, BufferUsage::AttrNormal)) {
      message = "Material has normal map, but mesh is missing vertex normals";
      return false;
    }
  }

  if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)
    || spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {

    if (!hasAttribute(spec.meshFeatures.vertexLayout, BufferUsage::AttrTexCoord)) {
      message = "Material has texture or normal map, but mesh is missing UVs";
      return false;
    }
  }

  return true;
}

MaterialFeatures::Enum parseMaterialFlagName(const std::string& name)
{
  if (name == "has-transparency") {
    return MaterialFeatures::HasTransparency;
  }
  else if (name == "has-texture") {
    return MaterialFeatures::HasTexture;
  }
  else if (name == "has-normal-map") {
    return MaterialFeatures::HasNormalMap;
  }
  else if (name == "has-cube-map") {
    return MaterialFeatures::HasCubeMap;
  }
  else if (name == "is-double-sided") {
    return MaterialFeatures::IsDoubleSided;
  }

  EXCEPTION("Unrecognised flag '" << name << "'");
}

template<typename T_FLAG, typename T_FLAGS>
void computePermutations(const std::vector<T_FLAG>& wildcards, size_t index,
  const T_FLAGS& baseFlags, std::vector<T_FLAGS>& flagSets)
{
  assert(index < wildcards.size());

  auto flag = wildcards[index];

  T_FLAGS flagSetA = baseFlags;
  T_FLAGS flagSetB = baseFlags;

  flagSetA.set(flag, true);
  flagSetB.set(flag, false);

  if (index + 1 == wildcards.size()) {
    flagSets.push_back(flagSetA);
    flagSets.push_back(flagSetB);
  }
  else {
    computePermutations(wildcards, index + 1, flagSetA, flagSets);
    computePermutations(wildcards, index + 1, flagSetB, flagSets);
  }
}

template<typename T_FLAG, typename T_FLAGS>
std::vector<T_FLAGS> parseFlags(const XmlNode& flagsXml,
  const std::function<T_FLAG(const std::string&)>& stringToFlag)
{
  std::vector<T_FLAGS> flagSets;
  std::vector<T_FLAG> wildcards;
  T_FLAGS baseFlags;

  for (auto& flagXml : flagsXml) {
    auto flag = stringToFlag(flagXml.name());
    auto flagValue = flagXml.contents();

    if (flagValue == "*") {
      wildcards.push_back(flag);
    }
    else {
      baseFlags.set(flag, flagValue == "true");
    }
  }

  if (wildcards.size() > 0) {
    computePermutations(wildcards, 0, baseFlags, flagSets);
  }
  else {
    flagSets.push_back(baseFlags);
  }

  return flagSets;
}

std::vector<MaterialFeatureSet> parseMaterialFeaturesXml(const XmlNode& materialFeaturesXml)
{
  std::vector<MaterialFeatureSet> featureSets;

  auto stringToFlag = [](const std::string& name) { return parseMaterialFlagName(name); };

  auto flagsXml = materialFeaturesXml.child("flags");

  auto flagSets = parseFlags<
    MaterialFeatures::Enum,
    MaterialFeatures::Flags
  >(*flagsXml, stringToFlag);

  for (auto flags : flagSets) {
    featureSets.push_back(MaterialFeatureSet{
      .flags = flags
    });
  }

  return featureSets;
}

BufferUsage parseAttributeName(const std::string& name)
{
  if (name == "position") {
    return BufferUsage::AttrPosition;
  }
  else if (name == "normal") {
    return BufferUsage::AttrNormal;
  }
  else if (name == "uv") {
    return BufferUsage::AttrTexCoord;
  }
  else if (name == "tangent") {
    return BufferUsage::AttrTangent;
  }
  else if (name == "joint-indices") {
    return BufferUsage::AttrJointIndices;
  }
  else if (name == "joint-weights") {
    return BufferUsage::AttrJointWeights;
  }

  EXCEPTION("Unrecognised vertex attribute '" << name << "'");
}

VertexLayout makeLayout(const std::set<BufferUsage>& attributes)
{
  VertexLayout layout{};

  assert(attributes.size() <= layout.size());

  size_t i = 0;
  for (auto attr : attributes) {
    layout[i++] = attr;
  }

  return layout;
}

void computePermutations(const std::vector<BufferUsage>& wildcards, size_t index,
  const std::set<BufferUsage>& baseLayout, std::vector<VertexLayout>& layouts)
{
  assert(index < wildcards.size());

  auto attribute = wildcards[index];

  auto layoutA = baseLayout;
  layoutA.insert(attribute);
  auto layoutB = baseLayout;

  if (index + 1 == wildcards.size()) {
    layouts.push_back(makeLayout(layoutA));
    layouts.push_back(makeLayout(layoutB));
  }
  else {
    computePermutations(wildcards, index + 1, layoutA, layouts);
    computePermutations(wildcards, index + 1, layoutB, layouts);
  }
}

std::vector<VertexLayout> parseVertexLayout(const XmlNode& vertexLayoutXml)
{
  std::vector<BufferUsage> wildcards;
  std::set<BufferUsage> baseLayout;

  for (auto& attributeXml : vertexLayoutXml) {
    auto attrName = attributeXml.name();
    auto attr = parseAttributeName(attrName);
    auto value = attributeXml.contents();

    if (value == "*") {
      wildcards.push_back(attr);
    }
    else if (value == "true") {
      baseLayout.insert(attr);
    }
  }

  std::vector<VertexLayout> layouts;

  if (wildcards.size() > 0) {
    computePermutations(wildcards, 0, baseLayout, layouts);
  }
  else {
    layouts.push_back(makeLayout(baseLayout));
  }

  return layouts;
}

MeshFeatures::Enum parseMeshFlagName(const std::string& name)
{
  if (name == "is-instanced") {
    return MeshFeatures::IsInstanced;
  }
  else if (name == "is-skybox") {
    return MeshFeatures::IsSkybox;
  }
  else if (name == "is-terrain") {
    return MeshFeatures::IsTerrain;
  }
  else if (name == "is-animated") {
    return MeshFeatures::IsAnimated;
  }
  else if (name == "has-tangents") {
    EXCEPTION("No need to specify has-tangents as it's inferred");
    //return MeshFeatures::HasTangents;
  }
  else if (name == "casts-shadow") {
    return MeshFeatures::CastsShadow;
  }
  else if (name == "is-quad") {
    return MeshFeatures::IsQuad;
  }
  else if (name == "is-dynamic-text") {
    return MeshFeatures::IsDynamicText;
  }

  EXCEPTION("Unrecognised flag '" << name << "'");
}

std::vector<MeshFeatureSet> parseMeshFeaturesXml(const XmlNode& meshFeaturesXml)
{
  auto vertexLayouts = parseVertexLayout(*meshFeaturesXml.child("vertex-layout"));

  auto stringToFlag = [](const std::string& name) { return parseMeshFlagName(name); };

  auto flagSets = parseFlags<
    MeshFeatures::Enum,
    MeshFeatures::Flags
  >(*meshFeaturesXml.child("flags"), stringToFlag);

  std::vector<MeshFeatureSet> meshFeatureSets;

  for (auto& vertexLayout : vertexLayouts) {
    for (auto& flags : flagSets) {
      meshFeatureSets.push_back(MeshFeatureSet{
        .vertexLayout = vertexLayout,
        .flags = flags
      });
    }
  }

  return meshFeatureSets;
}

void setInferred(ShaderProgramSpec& spec)
{
  if (hasAttribute(spec.meshFeatures.vertexLayout, BufferUsage::AttrTangent)) {
    spec.meshFeatures.flags.set(MeshFeatures::HasTangents, true);
  }
  // ...
}

std::vector<RenderPass> parseRenderPassesXml(const XmlNode& renderPassesXml)
{
  std::vector<RenderPass> renderPasses;

  for (auto& renderPassXml : renderPassesXml) {
    ASSERT(renderPassXml.name() == "render-pass", "Expected <render-pass> element");
    if (renderPassXml.contents() == "overlay") {
      renderPasses.push_back(RenderPass::Overlay);
    }
    else if (renderPassXml.contents() == "main") {
      renderPasses.push_back(RenderPass::Main);
    }
    else if (renderPassXml.contents() == "shadow") {
      renderPasses.push_back(RenderPass::Shadow);
    }
  }

  return renderPasses;
}

void parseShaderSpec(const XmlNode& shaderXml, std::vector<ShaderProgramSpec>& specs,
  Logger& logger)
{
  auto renderPassesXml = shaderXml.child("render-passes");
  auto meshFeaturesXml = shaderXml.child("mesh-features");
  auto materialFeaturesXml = shaderXml.child("material-features");

  auto renderPasses = parseRenderPassesXml(*renderPassesXml);
  auto meshFeatureSets = parseMeshFeaturesXml(*meshFeaturesXml);
  auto materialFeatureSets = parseMaterialFeaturesXml(*materialFeaturesXml);

  for (auto renderPass : renderPasses) {
    for (auto& meshFeatures : meshFeatureSets) {
      for (auto& materialFeatures : materialFeatureSets) {
        ShaderProgramSpec spec{
          .renderPass = renderPass,
          .meshFeatures = meshFeatures,
          .materialFeatures = materialFeatures
        };

        setInferred(spec);

        std::string message;
        if (isValid(spec, message)) {
          specs.push_back(spec);
        }
        else {
          logger.warn(STR("Skipping invalid shader spec: " << message));
        }
      }
    }
  }
}

std::vector<ShaderProgramSpec> parseShadersXml(const XmlNode& shadersXml, Logger& logger)
{
  ASSERT(shadersXml.name() == "shaders", "Expected <shaders> element");

  std::vector<ShaderProgramSpec> specs;

  for (auto& node : shadersXml) {
    parseShaderSpec(node, specs, logger);
  }

  return specs;
}

} // namespace

std::vector<ShaderProgramSpec> parseShaderManifest(const std::vector<char>& data, Logger& logger)
{
  auto root = parseXml(data);
  return parseShadersXml(*root, logger);
}

} // namespace lithic3d

#pragma once

#include "lithic3d/renderer.hpp"

namespace lithic3d
{
namespace render
{

using ShaderByteCode = std::vector<uint32_t>;

struct ShaderProgram
{
  ShaderByteCode vertexShaderCode;
  ShaderByteCode fragmentShaderCode;
};

enum class ShaderType
{
  Vertex = 0,
  Fragment
};

struct ShaderProgramSpec
{
  RenderPass renderPass;  // TODO: Should this be here?
  MeshFeatureSet meshFeatures;
  MaterialFeatureSet materialFeatures;

  bool operator==(const ShaderProgramSpec& rhs) const = default;

  std::string toString() const;
};

ShaderProgram loadShaderProgram(const FileSystem& fileSystem, const ShaderProgramSpec& spec);
std::string fileNameForShader(ShaderType type, const ShaderProgramSpec& spec);

} // namespace render
} // namespace lithic3d

template<>
struct std::hash<lithic3d::render::ShaderProgramSpec>
{
  std::size_t operator()(const lithic3d::render::ShaderProgramSpec& spec) const noexcept
  {
    return lithic3d::hashAll(spec.renderPass, spec.meshFeatures, spec.materialFeatures);
  }
};

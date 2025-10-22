#pragma once

#include "renderer.hpp"

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

template<>
struct std::hash<render::ShaderProgramSpec>
{
  std::size_t operator()(const render::ShaderProgramSpec& spec) const noexcept
  {
    return hashAll(spec.renderPass, spec.meshFeatures, spec.materialFeatures);
  }
};

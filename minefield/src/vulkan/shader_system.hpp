#pragma once

#include "renderer.hpp"

class Logger;
class FileSystem;

namespace render
{

struct ShaderProgram
{
  std::vector<uint32_t> vertexShaderCode;
  std::vector<uint32_t> fragmentShaderCode;
};

enum class ShaderType
{
  Vertex,
  Fragment
};

struct ShaderSpec
{
  RenderPass renderPass;
  MeshFeatureSet meshFeatures;
  MaterialFeatureSet materialFeatures;

  bool operator==(const ShaderSpec& rhs) const = default;
};

class ShaderSystem
{
  public:
    virtual ShaderProgram compileShader(const ShaderSpec& spec) = 0;

    virtual ~ShaderSystem() = default;
};

using ShaderSystemPtr = std::unique_ptr<ShaderSystem>;

ShaderSystemPtr createShaderSystem(FileSystem& fileSystem, Logger& logger);

} // namespace render

template<>
struct std::hash<render::ShaderSpec>
{
  std::size_t operator()(const render::ShaderSpec& spec) const noexcept
  {
    return hashAll(spec.renderPass, spec.meshFeatures, spec.materialFeatures);
  }
};

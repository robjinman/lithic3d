#pragma once

#include "renderer.hpp"

class Logger;
class FileSystem;

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
  Vertex,
  Fragment
};

struct ShaderProgramSpec
{
  RenderPass renderPass;
  MeshFeatureSet meshFeatures;
  MaterialFeatureSet materialFeatures;

  bool operator==(const ShaderProgramSpec& rhs) const = default;
};

class ShaderSystem
{
  public:
    virtual ShaderProgram compileShaderProgram(const ShaderProgramSpec& spec) = 0;

    virtual ~ShaderSystem() = default;
};

using ShaderSystemPtr = std::unique_ptr<ShaderSystem>;

ShaderSystemPtr createShaderSystem(FileSystem& fileSystem, Logger& logger);

} // namespace render

template<>
struct std::hash<render::ShaderProgramSpec>
{
  std::size_t operator()(const render::ShaderProgramSpec& spec) const noexcept
  {
    return hashAll(spec.renderPass, spec.meshFeatures, spec.materialFeatures);
  }
};

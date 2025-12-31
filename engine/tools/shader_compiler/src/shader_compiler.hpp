#pragma once

#include <lithic3d/vulkan/shader.hpp>
#include <filesystem>

namespace lithic3d
{
namespace tools
{

class ShaderCompiler
{
  public:
    virtual render::ShaderProgram compileShaderProgram(const render::ShaderProgramSpec& spec) = 0;

    virtual ~ShaderCompiler() = default;
};

using ShaderCompilerPtr = std::unique_ptr<ShaderCompiler>;

ShaderCompilerPtr createShaderCompiler(const std::filesystem::path& sourcesDir,
  const std::filesystem::path& outputDir);

} // namespace tools
} // namespace lithic3d

#pragma once

#include <lithic3d/vulkan/shader.hpp>
#include <filesystem>

class ShaderCompiler
{
  public:
    virtual lithic3d::render::ShaderProgram compileShaderProgram(
      const lithic3d::render::ShaderProgramSpec& spec) = 0;

    virtual ~ShaderCompiler() = default;
};

using ShaderCompilerPtr = std::unique_ptr<ShaderCompiler>;

ShaderCompilerPtr createShaderCompiler(const std::filesystem::path& sourcesDir,
  const std::filesystem::path& outputDir);

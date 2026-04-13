#pragma once

#include "../renderer.hpp"
#include "../file_system.hpp"

namespace lithic3d
{
namespace render
{

enum class ShaderType
{
  Vertex = 0,
  Fragment
};

using ShaderByteCode = std::vector<uint32_t>;

struct ShaderProgram
{
  ShaderByteCode vertexShaderCode;
  ShaderByteCode fragmentShaderCode;
};

ShaderProgram loadShaderProgram(DirectoryPtr directory, const ShaderProgramSpec& spec);
std::string fileNameForShader(ShaderType type, const ShaderProgramSpec& spec);

} // namespace render
} // namespace lithic3d

#pragma once

#include "lithic3d/renderer.hpp"

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

ShaderProgram loadShaderProgram(const FileSystem& fileSystem, const ShaderProgramSpec& spec);
std::string fileNameForShader(ShaderType type, const ShaderProgramSpec& spec);

} // namespace render
} // namespace lithic3d

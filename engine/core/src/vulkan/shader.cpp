#include "shader.hpp"
#include "file_system.hpp"

namespace render
{
namespace
{

void loadShader(const FileSystem& fileSystem, ShaderType type, const ShaderProgramSpec& spec,
  ShaderByteCode& code)
{
  auto path = STR("shaders/" << static_cast<int>(type) << "_" << spec.toString() << ".spv");
  auto bytes = fileSystem.readAppDataFile(path);
  code.resize(bytes.size() / sizeof(uint32_t));
  memcpy(code.data(), bytes.data(), bytes.size());
}

}

std::string ShaderProgramSpec::toString() const
{
  return STR(static_cast<int>(renderPass) << "_" << meshFeatures << "_" << materialFeatures);
}

ShaderProgram loadShaderProgram(const FileSystem& fileSystem, const ShaderProgramSpec& spec)
{
  ShaderProgram program;

  loadShader(fileSystem, ShaderType::Vertex, spec, program.vertexShaderCode);
  loadShader(fileSystem, ShaderType::Fragment, spec, program.fragmentShaderCode);

  return program;
}

}

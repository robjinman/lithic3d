#include "lithic3d/vulkan/shader.hpp"
#include "lithic3d/file_system.hpp"
#include "lithic3d/strings.hpp"
#include <cstring>

namespace lithic3d
{
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

} // namespace

std::string ShaderProgramSpec::toString() const
{
  std::stringstream ss;
  ss << "rp" << static_cast<int>(renderPass);
  ss << "vtl";
  size_t n = meshFeatures.vertexLayout.size();
  for (size_t i = 0; i < n; ++i) {
    ss << static_cast<int>(meshFeatures.vertexLayout[i]);
    if (i + 1 < n) {
      ss << "-";
    }
  }
  ss << "mshf" << meshFeatures.flags;
  ss << "matf" << materialFeatures.flags;
  return ss.str();
}

std::string fileNameForShader(ShaderType type, const ShaderProgramSpec& spec)
{
  return STR(static_cast<int>(type) << "_" << spec.toString() << ".spv");
}

ShaderProgram loadShaderProgram(const FileSystem& fileSystem, const ShaderProgramSpec& spec)
{
  ShaderProgram program;

  loadShader(fileSystem, ShaderType::Vertex, spec, program.vertexShaderCode);
  loadShader(fileSystem, ShaderType::Fragment, spec, program.fragmentShaderCode);

  return program;
}

} // namespace render
} // namespace lithic3d

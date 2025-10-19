#include "shader_system.hpp"
#include "file_system.hpp"
#include "logger.hpp"
#include <shaderc/shaderc.hpp>
#include <cassert>
#include <cstring>

namespace render
{

namespace
{

class SourceIncluder : public shaderc::CompileOptions::IncluderInterface
{
  public:
    SourceIncluder(const FileSystem& fileSystem)
      : m_fileSystem(fileSystem) {}

    shaderc_include_result* GetInclude(const char* requested_source,
      shaderc_include_type type, const char* requesting_source, size_t include_depth) override;

    void ReleaseInclude(shaderc_include_result* data) override;

  private:
    const FileSystem& m_fileSystem;
    std::string m_errorMessage;
};

shaderc_include_result* SourceIncluder::GetInclude(const char* requested_source,
  shaderc_include_type, const char*, size_t)
{
  auto result = new shaderc_include_result{};

  try {
    const std::filesystem::path sourcesDir = "shaders";
    auto sourcePath = sourcesDir / requested_source;

    size_t sourceNameLength = sourcePath.string().length();
    char* nameBuffer = new char[sourceNameLength];
    memcpy(nameBuffer, reinterpret_cast<const char*>(sourcePath.c_str()), sourceNameLength);

    result->source_name = nameBuffer;
    result->source_name_length = sourceNameLength;

    auto source = m_fileSystem.readAppDataFile(sourcePath);
    size_t contentBufferLength = source.size();
    char* contentBuffer = new char[contentBufferLength];
    memcpy(contentBuffer, source.data(), source.size());

    result->content = contentBuffer;
    result->content_length = contentBufferLength;
    result->user_data = nullptr;
  }
  catch (const std::exception& ex) {
    m_errorMessage = ex.what();
    result->content = m_errorMessage.c_str();
    result->content_length = m_errorMessage.length();
  }

  return result;
}

void SourceIncluder::ReleaseInclude(shaderc_include_result* data)
{
  if (data) {
    if (data->content) {
      delete[] data->content;
    }
    if (data->source_name) {
      delete[] data->source_name;
    }
    delete data;
  }
}

class ShaderSystemImpl : public ShaderSystem
{
  public:
    ShaderSystemImpl(FileSystem& fileSystem, Logger& logger);

    ShaderProgram compileShader(const ShaderSpec& spec) override;

  private:
    Logger& m_logger;
    FileSystem& m_fileSystem;

    std::vector<uint32_t> compileShader(const std::string& name, const std::vector<char>& source,
      ShaderType type, const std::vector<std::string>& defines);
};

ShaderSystemImpl::ShaderSystemImpl(FileSystem& fileSystem, Logger& logger)
  : m_fileSystem(fileSystem)
  , m_logger(logger)
{
}

ShaderProgram ShaderSystemImpl::compileShader(const ShaderSpec& spec)
{
  std::vector<std::string> defines;
  for (auto attr : spec.meshFeatures.vertexLayout) {
    switch (attr) {
      case BufferUsage::AttrPosition: defines.push_back("ATTR_POSITION"); break;
      case BufferUsage::AttrNormal: defines.push_back("ATTR_NORMAL"); break;
      case BufferUsage::AttrTexCoord: defines.push_back("ATTR_TEXCOORD"); break;
      case BufferUsage::AttrTangent: defines.push_back("ATTR_TANGENT"); break;
      case BufferUsage::AttrJointIndices: defines.push_back("ATTR_JOINTS"); break;
      case BufferUsage::AttrJointWeights: defines.push_back("ATTR_WEIGHTS"); break;
      default: break;
    }
  }

  std::string vertShader = "main_default";
  std::string fragShader = "main_default";

  if (spec.meshFeatures.flags.test(MeshFeatures::IsInstanced)) {
    defines.push_back("ATTR_MODEL_MATRIX");
  }

  if (spec.meshFeatures.flags.test(MeshFeatures::IsAnimated)) {
    defines.push_back("FEATURE_VERTEX_SKINNING");
  }

  if (spec.renderPass == RenderPass::Shadow) {
    defines.push_back("RENDER_PASS_SHADOW");
    fragShader = "main_depth";
  }
  else {
    defines.push_back("FEATURE_MATERIALS");

    if (spec.renderPass == RenderPass::Main) {
      defines.push_back("FEATURE_LIGHTING");

      if (spec.meshFeatures.flags.test(MeshFeatures::IsSkybox)) {
        vertShader = "main_passthrough";
        fragShader = "main_skybox";
      }
    }

    if (spec.meshFeatures.flags.test(MeshFeatures::IsQuad)) {
      if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
        vertShader = "main_sprite";
        fragShader = "main_sprite";
      }
      else {
        vertShader = "main_quad";
        fragShader = "main_quad";
      }
    }
    else if (spec.meshFeatures.flags.test(MeshFeatures::IsDynamicText)) {
      vertShader = "main_dynamic_text";
      fragShader = "main_sprite";
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {
      assert(spec.meshFeatures.flags.test(MeshFeatures::HasTangents));
      defines.push_back("FEATURE_NORMAL_MAPPING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      defines.push_back("FEATURE_TEXTURE_MAPPING");
    }
  }

  m_logger.info(STR("Compiling shaders with options: " << defines));
  m_logger.info(STR("Render pass: " << static_cast<int>(spec.renderPass)));
  m_logger.info(STR("Mesh features: " << spec.meshFeatures));
  m_logger.info(STR("Material features: " << spec.materialFeatures));

  ShaderProgram program;

  auto vertShaderSrc =
    m_fileSystem.readAppDataFile(STR("shaders/vertex/" << vertShader << ".glsl"));
  auto fragShaderSrc =
    m_fileSystem.readAppDataFile(STR("shaders/fragment/" << fragShader << ".glsl"));
  program.vertexShaderCode = compileShader("vertex", vertShaderSrc, ShaderType::Vertex, defines);
  program.fragmentShaderCode = compileShader("fragment", fragShaderSrc, ShaderType::Fragment,
    defines);

  assert(program.fragmentShaderCode.size() > 0);
  assert(program.vertexShaderCode.size() > 0);

  return program;
}

std::vector<uint32_t> ShaderSystemImpl::compileShader(const std::string& name,
  const std::vector<char>& source, ShaderType type, const std::vector<std::string>& defines)
{
  shaderc_shader_kind kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;
  switch (type) {
    case ShaderType::Vertex: kind = shaderc_shader_kind::shaderc_glsl_vertex_shader; break;
    case ShaderType::Fragment: kind = shaderc_shader_kind::shaderc_glsl_fragment_shader; break;
  }

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetWarningsAsErrors();
  options.SetIncluder(std::make_unique<SourceIncluder>(m_fileSystem));
  for (auto& define : defines) {
    options.AddMacroDefinition(define);
  }

  auto result = compiler.CompileGlslToSpv(source.data(), source.size(), kind, name.c_str(),
    options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    EXCEPTION("Error compiling shader: " << result.GetErrorMessage());
  }

  std::vector<uint32_t> code;
  code.assign(result.cbegin(), result.cend());

  return code;
}

} // namespace

ShaderSystemPtr createShaderSystem(FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<ShaderSystemImpl>(fileSystem, logger);
}

} // namespace render

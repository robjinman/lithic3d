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

struct ShaderSource
{
  ShaderType type;
  std::string name;
  std::string fileName;
  std::vector<std::string> defines;
  std::vector<char> source;
};

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

    ShaderProgram compileShaderProgram(const ShaderProgramSpec& spec) override;

  private:
    Logger& m_logger;
    FileSystem& m_fileSystem;

    ShaderByteCode compileShader(const ShaderSource& source);

    ShaderByteCode fetchShaderFromCache(ShaderType type, const ShaderProgramSpec& spec) const;
    ShaderSource loadVertShaderSource(const ShaderProgramSpec& spec) const;
    ShaderSource loadFragShaderSource(const ShaderProgramSpec& spec) const;
    std::string selectVertShader(const ShaderProgramSpec& spec) const;
    std::string selectFragShader(const ShaderProgramSpec& spec) const;
};

ShaderSystemImpl::ShaderSystemImpl(FileSystem& fileSystem, Logger& logger)
  : m_fileSystem(fileSystem)
  , m_logger(logger)
{
}

std::string ShaderSystemImpl::selectVertShader(const ShaderProgramSpec& spec) const
{
  std::string shader = "main_default";

  if (spec.renderPass == RenderPass::Main) {
    if (spec.meshFeatures.flags.test(MeshFeatures::IsSkybox)) {
      shader = "main_passthrough";
    }
  }

  if (spec.meshFeatures.flags.test(MeshFeatures::IsQuad)) {
    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      shader = "main_sprite";
    }
    else {
      shader = "main_quad";
    }
  }
  else if (spec.meshFeatures.flags.test(MeshFeatures::IsDynamicText)) {
    shader = "main_dynamic_text";
  }

  return shader;
}

std::string ShaderSystemImpl::selectFragShader(const ShaderProgramSpec& spec) const
{
  std::string shader = "main_default";

  if (spec.renderPass == RenderPass::Shadow) {
    shader = "main_depth";
  }
  else {
    if (spec.renderPass == RenderPass::Main) {
      if (spec.meshFeatures.flags.test(MeshFeatures::IsSkybox)) {
        shader = "main_skybox";
      }
    }

    if (spec.meshFeatures.flags.test(MeshFeatures::IsQuad)) {
      if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
        shader = "main_sprite";
      }
      else {
        shader = "main_quad";
      }
    }
    else if (spec.meshFeatures.flags.test(MeshFeatures::IsDynamicText)) {
      shader = "main_sprite";
    }
  }

  return shader;
}

ShaderSource ShaderSystemImpl::loadVertShaderSource(const ShaderProgramSpec& spec) const
{
  ShaderSource source;
  source.name = "vertex";
  source.type = ShaderType::Vertex;
  source.fileName = selectVertShader(spec);
  source.source = m_fileSystem.readAppDataFile(STR("shaders/vertex/"
    << source.fileName << ".glsl"));

  for (auto attr : spec.meshFeatures.vertexLayout) {
    switch (attr) {
      case BufferUsage::AttrPosition: source.defines.push_back("ATTR_POSITION"); break;
      case BufferUsage::AttrNormal: source.defines.push_back("ATTR_NORMAL"); break;
      case BufferUsage::AttrTexCoord: source.defines.push_back("ATTR_TEXCOORD"); break;
      case BufferUsage::AttrTangent: source.defines.push_back("ATTR_TANGENT"); break;
      case BufferUsage::AttrJointIndices: source.defines.push_back("ATTR_JOINTS"); break;
      case BufferUsage::AttrJointWeights: source.defines.push_back("ATTR_WEIGHTS"); break;
      default: break;
    }
  }

  if (spec.meshFeatures.flags.test(MeshFeatures::IsInstanced)) {
    source.defines.push_back("ATTR_MODEL_MATRIX");
  }

  if (spec.meshFeatures.flags.test(MeshFeatures::IsAnimated)) {
    source.defines.push_back("FEATURE_VERTEX_SKINNING");
  }

  if (spec.renderPass == RenderPass::Shadow) {
    source.defines.push_back("RENDER_PASS_SHADOW");
  }
  else {
    source.defines.push_back("FEATURE_MATERIALS");

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {
      assert(spec.meshFeatures.flags.test(MeshFeatures::HasTangents));
      source.defines.push_back("FEATURE_NORMAL_MAPPING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      source.defines.push_back("FEATURE_TEXTURE_MAPPING");
    }
  }

  return source;
}

ShaderSource ShaderSystemImpl::loadFragShaderSource(const ShaderProgramSpec& spec) const
{
  ShaderSource source;
  source.name = "fragment";
  source.type = ShaderType::Fragment;
  source.fileName = selectFragShader(spec);
  source.source = m_fileSystem.readAppDataFile(STR("shaders/fragment/"
    << source.fileName << ".glsl"));

  if (spec.renderPass == RenderPass::Shadow) {
    source.defines.push_back("RENDER_PASS_SHADOW");
  }
  else {
    source.defines.push_back("FEATURE_MATERIALS");

    if (spec.renderPass == RenderPass::Main) {
      source.defines.push_back("FEATURE_LIGHTING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {
      assert(spec.meshFeatures.flags.test(MeshFeatures::HasTangents));
      source.defines.push_back("FEATURE_NORMAL_MAPPING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      source.defines.push_back("FEATURE_TEXTURE_MAPPING");
    }
  }

  return source;
}

ShaderByteCode ShaderSystemImpl::fetchShaderFromCache(ShaderType type,
  const ShaderProgramSpec& spec) const
{
  // TODO
  return ShaderByteCode{};
}

ShaderProgram ShaderSystemImpl::compileShaderProgram(const ShaderProgramSpec& spec)
{
  ShaderProgram program{
    .vertexShaderCode = fetchShaderFromCache(ShaderType::Vertex, spec),
    .fragmentShaderCode = fetchShaderFromCache(ShaderType::Fragment, spec)
  };

  if (program.vertexShaderCode.empty()) {
    auto source = loadVertShaderSource(spec);
    program.vertexShaderCode = compileShader(source);
  }

  if (program.fragmentShaderCode.empty()) {
    auto source = loadFragShaderSource(spec);
    program.fragmentShaderCode = compileShader(source);
  }

  assert(program.fragmentShaderCode.size() > 0);
  assert(program.vertexShaderCode.size() > 0);

  return program;
}

ShaderByteCode ShaderSystemImpl::compileShader(const ShaderSource& source)
{
  shaderc_shader_kind kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;
  switch (source.type) {
    case ShaderType::Vertex: kind = shaderc_shader_kind::shaderc_glsl_vertex_shader; break;
    case ShaderType::Fragment: kind = shaderc_shader_kind::shaderc_glsl_fragment_shader; break;
  }

  m_logger.info(STR("Compiling " << source.name << " shader with options: " << source.defines));

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetWarningsAsErrors();
  options.SetIncluder(std::make_unique<SourceIncluder>(m_fileSystem));
  for (auto& define : source.defines) {
    options.AddMacroDefinition(define);
  }

  auto result = compiler.CompileGlslToSpv(source.source.data(), source.source.size(), kind,
    source.name.c_str(), options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    EXCEPTION("Error compiling shader: " << result.GetErrorMessage());
  }

  ShaderByteCode code;
  code.assign(result.cbegin(), result.cend());

  return code;
}

} // namespace

ShaderSystemPtr createShaderSystem(FileSystem& fileSystem, Logger& logger)
{
  return std::make_unique<ShaderSystemImpl>(fileSystem, logger);
}

} // namespace render

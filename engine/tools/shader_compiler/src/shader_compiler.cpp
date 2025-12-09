#include "shader_compiler.hpp"
#include <lithic3d/vulkan/shader.hpp>
#include <lithic3d/utils.hpp>
#include <shaderc/shaderc.hpp>
#include <cstring>
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;

using namespace lithic3d;
using namespace lithic3d::render;

namespace
{

class SourceIncluder : public shaderc::CompileOptions::IncluderInterface
{
  public:
    SourceIncluder(const fs::path& sourcesDir)
      : m_sourcesDir(sourcesDir) {}

    shaderc_include_result* GetInclude(const char* requestedSource,
      shaderc_include_type type, const char* requestingSource, size_t includeDepth) override;

    void ReleaseInclude(shaderc_include_result* data) override;

  private:
    fs::path m_sourcesDir;
    std::string m_errorMessage;
};

shaderc_include_result* SourceIncluder::GetInclude(const char* requestedSource,
  shaderc_include_type, const char*, size_t)
{
  auto result = new shaderc_include_result{};

  try {
    auto sourcePath = m_sourcesDir / requestedSource;

    size_t sourceNameLength = sourcePath.string().length();
    char* nameBuffer = new char[sourceNameLength];
    memcpy(nameBuffer, reinterpret_cast<const char*>(sourcePath.c_str()), sourceNameLength);

    result->source_name = nameBuffer;
    result->source_name_length = sourceNameLength;

    auto source = readBinaryFile(sourcePath);
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

struct ShaderSource
{
  ShaderType type;
  std::string name;
  std::string fileName;
  std::vector<std::string> defines;
  std::vector<char> source;
};

class ShaderCompilerImpl : public ShaderCompiler
{
  public:
    ShaderCompilerImpl(const std::filesystem::path& sourcesDir, const std::filesystem::path& outputDir);

    ShaderProgram compileShaderProgram(const ShaderProgramSpec& spec);

  private:
    std::filesystem::path m_sourcesDir;
    std::filesystem::path m_outputDir;

    ShaderByteCode compileShader(const ShaderSource& source);
    ShaderSource loadVertShaderSource(const ShaderProgramSpec& spec) const;
    ShaderSource loadFragShaderSource(const ShaderProgramSpec& spec) const;
    std::string selectVertShader(const ShaderProgramSpec& spec) const;
    std::string selectFragShader(const ShaderProgramSpec& spec) const;
    void writeCompiledShader(ShaderType type, const ShaderProgramSpec& spec,
      const ShaderByteCode& code);
};

ShaderCompilerImpl::ShaderCompilerImpl(const fs::path& sourcesDir, const fs::path& outputDir)
  : m_sourcesDir(sourcesDir)
  , m_outputDir(outputDir)
{
}

std::string ShaderCompilerImpl::selectVertShader(const ShaderProgramSpec& spec) const
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

std::string ShaderCompilerImpl::selectFragShader(const ShaderProgramSpec& spec) const
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

ShaderSource ShaderCompilerImpl::loadVertShaderSource(const ShaderProgramSpec& spec) const
{
  ShaderSource source;
  source.name = "vertex";
  source.type = ShaderType::Vertex;
  source.fileName = selectVertShader(spec);
  source.source = readBinaryFile(m_sourcesDir / "vertex" / (source.fileName + ".glsl"));

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
      //assert(spec.meshFeatures.flags.test(MeshFeatures::HasTangents));
      source.defines.push_back("FEATURE_NORMAL_MAPPING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      source.defines.push_back("FEATURE_TEXTURE_MAPPING");
    }
  }

  return source;
}

ShaderSource ShaderCompilerImpl::loadFragShaderSource(const ShaderProgramSpec& spec) const
{
  ShaderSource source;
  source.name = "fragment";
  source.type = ShaderType::Fragment;
  source.fileName = selectFragShader(spec);
  source.source = readBinaryFile(m_sourcesDir / "fragment" / (source.fileName + ".glsl"));

  if (spec.renderPass == RenderPass::Shadow) {
    source.defines.push_back("RENDER_PASS_SHADOW");
  }
  else {
    source.defines.push_back("FEATURE_MATERIALS");

    if (spec.renderPass == RenderPass::Main) {
      source.defines.push_back("FEATURE_LIGHTING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasNormalMap)) {
      //assert(spec.meshFeatures.flags.test(MeshFeatures::HasTangents));
      source.defines.push_back("FEATURE_NORMAL_MAPPING");
    }

    if (spec.materialFeatures.flags.test(MaterialFeatures::HasTexture)) {
      source.defines.push_back("FEATURE_TEXTURE_MAPPING");
    }
  }

  return source;
}

void ShaderCompilerImpl::writeCompiledShader(ShaderType type, const ShaderProgramSpec& spec,
  const ShaderByteCode& code)
{
  auto path = m_outputDir / fileNameForShader(type, spec);

  std::cout << "Writing to " << path << std::endl;
  writeBinaryFile(path, reinterpret_cast<const char*>(code.data()), code.size() * sizeof(uint32_t));
}

ShaderProgram ShaderCompilerImpl::compileShaderProgram(const ShaderProgramSpec& spec)
{
  ShaderProgram program;

  auto vertShaderSource = loadVertShaderSource(spec);
  program.vertexShaderCode = compileShader(vertShaderSource);
  writeCompiledShader(ShaderType::Vertex, spec, program.vertexShaderCode);

  auto fragShaderSource = loadFragShaderSource(spec);
  program.fragmentShaderCode = compileShader(fragShaderSource);
  writeCompiledShader(ShaderType::Fragment, spec, program.fragmentShaderCode);

  return program;
}

ShaderByteCode ShaderCompilerImpl::compileShader(const ShaderSource& source)
{
  std::cout << "Compiling " << source.name << " shader with defines: "
    << source.defines << std::endl;

  shaderc_shader_kind kind = shaderc_shader_kind::shaderc_glsl_vertex_shader;
  switch (source.type) {
    case ShaderType::Vertex: kind = shaderc_shader_kind::shaderc_glsl_vertex_shader; break;
    case ShaderType::Fragment: kind = shaderc_shader_kind::shaderc_glsl_fragment_shader; break;
  }

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  options.SetWarningsAsErrors();
  options.SetIncluder(std::make_unique<SourceIncluder>(m_sourcesDir));
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

ShaderCompilerPtr createShaderCompiler(const std::filesystem::path& sourcesDir,
  const std::filesystem::path& outputDir)
{
  return std::make_unique<ShaderCompilerImpl>(sourcesDir, outputDir);
}

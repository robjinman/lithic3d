#pragma once

#include "renderer.hpp"
#include <filesystem>

namespace lithic3d
{

class Logger;

std::vector<render::ShaderProgramSpec> parseShaderManifest(const std::vector<char>& data,
  Logger& logger);

} // namespace lithic3d

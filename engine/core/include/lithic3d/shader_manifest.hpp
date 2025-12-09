#pragma once

#include "renderer.hpp"
#include <filesystem>

namespace lithic3d
{

std::vector<render::ShaderProgramSpec> parseShaderManifest(const std::vector<char>& data);

} // namespace lithic3d

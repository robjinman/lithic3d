#pragma once

#include "file_system.hpp"

namespace lithic3d
{

struct GameDataPaths
{
  DirectoryPtr shadersDir = nullptr;    // Defaults to APP_DATA_DIR/shaders
  DirectoryPtr texturesDir = nullptr;   // Defaults to APP_DATA_DIR/textures
  DirectoryPtr soundsDir = nullptr;     // Defaults to APP_DATA_DIR/sounds
  DirectoryPtr prefabsDir = nullptr;    // Defaults to APP_DATA_DIR/prefabs
  DirectoryPtr modelsDir = nullptr;     // Defaults to APP_DATA_DIR/models
  DirectoryPtr worldsDir = nullptr;     // Defaults to APP_DATA_DIR/worlds
  FilePath shaderManifest{};            // Defaults to APP_DATA_DIR/shaders.xml
};

} // namespace lithic3d

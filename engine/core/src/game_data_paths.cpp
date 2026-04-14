#include "lithic3d/game_data_paths.hpp"

namespace lithic3d
{

void fillDefaultPaths(const FileSystem& fileSystem, GameDataPaths& paths)
{
  if (!paths.modelsDir) {
    paths.modelsDir = fileSystem.appDataDirectory()->subdirectory("models");
  }
  if (!paths.prefabsDir) {
    paths.prefabsDir = fileSystem.appDataDirectory()->subdirectory("prefabs");
  }
  if (!paths.shaderManifest.directory) {
    paths.shaderManifest = {
      .directory = fileSystem.appDataDirectory(),
      .subpath = "shaders.xml"
    };
  }
  if (!paths.shadersDir) {
    paths.shadersDir = fileSystem.appDataDirectory()->subdirectory("shaders");
  }
  if (!paths.soundsDir) {
    paths.soundsDir = fileSystem.appDataDirectory()->subdirectory("sounds");
  }
  if (!paths.texturesDir) {
    paths.texturesDir = fileSystem.appDataDirectory()->subdirectory("textures");
  }
  if (!paths.worldsDir) {
    paths.worldsDir = fileSystem.appDataDirectory()->subdirectory("worlds");
  }
}

} // namespace lithic3d

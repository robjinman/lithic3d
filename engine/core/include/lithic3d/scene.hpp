#pragma once

#include "math.hpp"
#include <memory>

namespace lithic3d
{

struct Scene
{
  Vec3f cameraStartPosition;
};

using ScenePtr = std::unique_ptr<Scene>;

class FileSystem;
class TerrainSystem;
class EntityFactory;
class ResourceManager;

ScenePtr loadScene(const std::string& name, FileSystem& fileSystem, TerrainSystem& terrainSystem,
  EntityFactory& entityFactory, ResourceManager& resourceManager);

} // namespace lithic3d

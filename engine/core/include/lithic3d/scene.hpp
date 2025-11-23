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

ScenePtr loadScene(const std::string& name, FileSystem& fileSystem);

} // namespace lithic3d

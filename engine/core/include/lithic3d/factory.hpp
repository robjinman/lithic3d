#pragma once

#include "renderables.hpp"
#include "lithic3d/component_store.hpp"
#include <filesystem>

namespace lithic3d
{
/*
// Factory for creating simple objects without offering much customisation. Useful for quick demos.
class Factory
{
  public:
    virtual render::MaterialHandle constructMaterial(const std::filesystem::path& texturePath) = 0;
    virtual render::MaterialHandle constructMaterial(const std::filesystem::path& texturePath,
      const std::filesystem::path& normalMapPath) = 0;

    virtual void constructCuboid(EntityId id, const render::MaterialHandle& material,
      const Vec3f& size, const Vec2f& materialSize) = 0;

    virtual ~Factory() = default;
};

using FactoryPtr = std::unique_ptr<Factory>;

class Ecs;
class FileSystem;

FactoryPtr createFactory(Ecs& ecs, FileSystem& fileSystem);
*/
} // namespace lithic3d

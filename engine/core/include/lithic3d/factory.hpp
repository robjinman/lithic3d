#pragma once

#include "renderables.hpp"
#include "entity_id.hpp"
#include <filesystem>

namespace lithic3d
{

struct MaterialHandle
{
  ResourceHandle resource;
  render::MaterialFeatureSet features;
};

struct MeshHandle
{
  ResourceHandle resource;
  render::MeshFeatureSet features;
};

class Ecs;
class RenderResourceLoader;

class Factory
{
  public:
    virtual MaterialHandle createMaterial(const std::filesystem::path& texturePath) = 0;
    virtual EntityId createCuboid(const Vec3f& size, MaterialHandle material,
      const Vec2f& textureSize) = 0;

    virtual ~Factory() = default;
};

using FactoryPtr = std::unique_ptr<Factory>;

FactoryPtr createFactory(Ecs& ecs, RenderResourceLoader& renderResourceLoader);

}

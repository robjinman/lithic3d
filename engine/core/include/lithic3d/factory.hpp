#pragma once

#include "renderables.hpp"
#include "entity_id.hpp"
#include <filesystem>

namespace lithic3d
{

class Factory
{
  public:
    virtual render::MaterialHandle createMaterialAsync(const std::filesystem::path& texturePath,
      bool genMipmaps) = 0;
    virtual EntityId createStaticCuboid(EntityId parentId, const Vec3f& sizeInMetres,
      render::MaterialHandle material, const Vec2f& textureSizeInMetres, float restitution,
      float friction) = 0;
    virtual EntityId createDynamicCuboid(EntityId parentId, const Vec3f& sizeInMetres,
      render::MaterialHandle material, const Vec2f& textureSizeInMetres, float inverseMass,
      float restitution, float friction) = 0;

    virtual ~Factory() = default;
};

using FactoryPtr = std::unique_ptr<Factory>;

class Ecs;
class ModelLoader;
class RenderResourceLoader;

FactoryPtr createFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader);

}

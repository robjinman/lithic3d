#pragma once

#include "renderables.hpp"
#include "entity_id.hpp"
#include <filesystem>

namespace lithic3d
{

class Factory
{
  public:
    virtual render::MaterialHandle createMaterialAsync(const std::filesystem::path& texturePath) = 0;
    virtual EntityId createStaticCuboid(const Vec3f& size, render::MaterialHandle material,
      const Vec2f& textureSize, float restitution, float friction) = 0;
    virtual EntityId createDynamicCuboid(const Vec3f& size, render::MaterialHandle material,
      const Vec2f& textureSize, float inverseMass, float restitution, float friction) = 0;

    virtual ~Factory() = default;
};

using FactoryPtr = std::unique_ptr<Factory>;

class Ecs;
class ModelLoader;
class RenderResourceLoader;

FactoryPtr createFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader);

}

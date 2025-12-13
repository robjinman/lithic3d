#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
#include "entity_id.hpp"

namespace lithic3d
{

class EntityFactory
{
  public:
    virtual ResourceHandle loadPrefabAsync(const std::string& name) = 0;
    virtual EntityId constructEntity(const std::string& type, const Mat4x4f& transform) const = 0;
    virtual bool hasPrefab(const std::string& name) const = 0;

    virtual ~EntityFactory() = default;
};

using EntityFactoryPtr = std::unique_ptr<EntityFactory>;

class Ecs;
class FileSystem;
class ModelLoader;
class RenderResourceLoader;

EntityFactoryPtr createEntityFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
  const FileSystem& fileSystem, Logger& logger);

} // namespace lithic3d

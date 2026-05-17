#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
#include "entity_id.hpp"

namespace lithic3d
{

class XmlNode;

class EntityFactory
{
  public:
    virtual ResourceHandle loadPrefabAsync(const std::string& name) = 0;
    virtual bool hasEntityType(const std::string& type) const = 0;
    virtual EntityId constructEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform) const = 0;
    virtual EntityId constructEntity(EntityId parentId, const XmlNode& xmlEntity) const = 0;
    virtual EntityId constructGhostEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform, const Vec4f& colour) = 0;
    virtual bool hasPrefab(const std::string& name) const = 0;

    virtual ~EntityFactory() = default;
};

using EntityFactoryPtr = std::unique_ptr<EntityFactory>;

class Ecs;
class ModelLoader;
class RenderResourceLoader;
struct GameDataPaths;

EntityFactoryPtr createEntityFactory(Ecs& ecs, ModelLoader& modelLoader,
  RenderResourceLoader& renderResourceLoader, ResourceManager& resourceManager,
  const GameDataPaths& paths, Logger& logger);

} // namespace lithic3d

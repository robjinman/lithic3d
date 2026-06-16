#pragma once

#include "math.hpp"
#include "resource_manager.hpp"
#include "entity_id.hpp"
#include "xml.hpp"
#include "systems.hpp"

namespace lithic3d
{

class EntityFactory
{
  public:
    virtual ResourceHandle loadPrefabAsync(const std::string& name) = 0;
    virtual bool hasEntityType(const std::string& type) const = 0;
    virtual EntityId constructEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform) const = 0;
    virtual EntityId constructEntity(EntityId parentId, const XmlNode& xmlEntity,
      std::array<bool, Systems::NUMBER_OF_SYSTEMS>& changedFromPrefab,
      std::vector<XmlNodePtr>& unused) const = 0;
    virtual EntityId constructGhostEntity(EntityId parentId, const std::string& type,
      const Mat4x4f& transform, const Vec4f& colour) = 0;
    virtual bool hasPrefab(const std::string& name) const = 0;

    virtual ~EntityFactory() = default;
};

using EntityFactoryPtr = std::unique_ptr<EntityFactory>;

class Ecs;
struct GameDataPaths;

EntityFactoryPtr createEntityFactory(Ecs& ecs, ResourceManager& resourceManager,
  const GameDataPaths& paths, Logger& logger);

} // namespace lithic3d

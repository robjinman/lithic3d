#pragma once

#include "component_store.hpp"
#include "math.hpp"
#include <memory>

namespace lithic3d
{

class XmlNode;

class EntityFactory
{
  public:
    // Any required resources must already be present in the resource manager
    virtual void loadPrefabs(const XmlNode& prefabs) = 0;

    // Construct entity from prefab with instance-specific customisation
    virtual EntityId constructEntity(const XmlNode& data, const Mat4x4f& transform) const = 0;

    virtual ~EntityFactory() = default;
};

using EntityFactoryPtr = std::unique_ptr<EntityFactory>;

class ResourceManager;

EntityFactoryPtr createEntityFactory(ResourceManager& resourceManager);

} // namespace lithic3d

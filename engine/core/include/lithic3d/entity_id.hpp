#pragma once

#include <memory>
#include <set>

namespace lithic3d
{

using EntityId = uint64_t;

constexpr EntityId NULL_ENTITY_ID = 0;

using EntityIdSet = std::set<EntityId>;

class EntityIdAllocator
{
  public:
    virtual EntityId getNewEntityId() = 0;

    virtual ~EntityIdAllocator() = default;
};

using EntityIdAllocatorPtr = std::unique_ptr<EntityIdAllocator>;

EntityIdAllocatorPtr createEntityIdAllocator(EntityId lastId);

} // namespace lithic3d

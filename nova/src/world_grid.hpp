#pragma once

#include "math.hpp"
#include "system.hpp"

// TODO: Make this a System?
class WorldGrid
{
  public:
    virtual void addEntity(uint32_t x, uint32_t y, EntityId entityId) = 0;
    virtual void removeEntity(uint32_t x, uint32_t y, EntityId entityId) = 0;
    virtual const std::set<EntityId>& getEntities(uint32_t x, uint32_t y) const = 0;
    virtual bool hasEntityAt(uint32_t x, uint32_t y, EntityId entityId) const = 0;
};

using WorldGridPtr = std::unique_ptr<WorldGrid>;

WorldGridPtr createWorldGrid();

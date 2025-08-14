#pragma once

#include "system.hpp"

class SysGrid : public System
{
  public:
    virtual void addEntity(EntityId entityId, uint32_t x, uint32_t y) = 0;
    virtual const std::set<EntityId>& getEntities(uint32_t x, uint32_t y) const = 0;
    virtual bool hasEntityAt(EntityId entityId, uint32_t x, uint32_t y) const = 0;
};

using SysGridPtr = std::unique_ptr<SysGrid>;

SysGridPtr createSysGrid();

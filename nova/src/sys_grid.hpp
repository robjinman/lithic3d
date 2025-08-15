#pragma once

#include "system.hpp"

class SysGrid : public System
{
  public:
    virtual void addEntity(EntityId entityId, int x, int y) = 0;
    virtual const std::set<EntityId>& getEntities(int x, int y) const = 0;
    virtual bool hasEntityAt(EntityId entityId, int x, int y) const = 0;
    virtual bool tryMove(EntityId entityId, int dx, int dy) = 0;
};

using SysGridPtr = std::unique_ptr<SysGrid>;

SysGridPtr createSysGrid(EventSystem& eventSystem);

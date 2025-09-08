#pragma once

#include "ecs.hpp"

class SysGrid : public System
{
  public:
    virtual void addEntity(EntityId entityId, int x, int y) = 0;
    virtual const EntityIdSet& getEntities(int x, int y) const = 0;
    virtual EntityIdSet getEntities(int minX, int minY, int maxX, int maxY) const = 0;
    virtual bool hasEntityAt(EntityId entityId, int x, int y) const = 0;
    virtual const Vec2i& entityPos(EntityId entityId) const = 0;
    virtual bool tryMove(EntityId entityId, int dx, int dy) = 0;
};

using SysGridPtr = std::unique_ptr<SysGrid>;

class EventSystem;

SysGridPtr createSysGrid(EventSystem& eventSystem);

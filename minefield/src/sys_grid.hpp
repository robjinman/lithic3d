#pragma once

#include <fge/systems.hpp>

const int GRID_W = 21;
const int GRID_H = 11;

const float GRID_CELL_W = 0.0625;
const float GRID_CELL_H = 0.0625;

// TODO: Move all system IDs to same file
const fge::SystemId GRID_SYSTEM = fge::LAST_SYSTEM_ID + 1;

class SysGrid : public fge::System
{
  public:
    virtual void addEntity(fge::EntityId entityId, int x, int y) = 0;
    virtual const fge::EntityIdSet& getEntities(int x, int y) const = 0;
    virtual fge::EntityIdSet getEntities(int minX, int minY, int maxX, int maxY) const = 0;
    virtual bool hasEntityAt(fge::EntityId entityId, int x, int y) const = 0;
    virtual const fge::Vec2i& entityPos(fge::EntityId entityId) const = 0;
    virtual bool tryMove(fge::EntityId entityId, int dx, int dy) = 0;
    virtual bool goTo(fge::EntityId entityId, int x, int y) = 0;
    virtual bool isInRange(int x, int y) const = 0;

    virtual ~SysGrid() = default;

    static const fge::SystemId id = GRID_SYSTEM;
};

using SysGridPtr = std::unique_ptr<SysGrid>;

namespace fge { class EventSystem; }

SysGridPtr createSysGrid(fge::EventSystem& eventSystem);

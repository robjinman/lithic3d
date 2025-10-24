#include "sys_grid.hpp"
#include "game_events.hpp"
#include <fge/math.hpp>
#include <array>
#include <set>
#include <map>

using fge::EntityId;
using fge::Vec2i;

namespace
{

class SysGridImpl : public SysGrid
{
  public:
    explicit SysGridImpl(fge::EventSystem& eventSystem)
      : m_eventSystem(eventSystem)
    {}

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(fge::Tick, const fge::InputState&) override {}
    void processEvent(const fge::Event& event) override {}

    void addEntity(EntityId entityId, int x, int y) override;
    const fge::EntityIdSet& getEntities(int x, int y) const override;
    fge::EntityIdSet getEntities(int minX, int minY, int maxX, int maxY) const override;
    bool hasEntityAt(EntityId entityId, int x, int y) const override;
    const Vec2i& entityPos(EntityId entityId) const override;
    bool tryMove(EntityId entityId, int dx, int dy) override;
    bool goTo(EntityId entityId, int x, int y) override;
    bool isInRange(int x, int y) const override;

  private:
    fge::EventSystem& m_eventSystem;
    std::array<std::array<fge::EntityIdSet, GRID_W>, GRID_H> m_cells;
    std::map<EntityId, Vec2i> m_entities;
};

bool SysGridImpl::isInRange(int x, int y) const
{
  return fge::inRange(x, 0, GRID_W - 1) && fge::inRange(y, 0, GRID_H - 1);
}

void SysGridImpl::removeEntity(EntityId entityId)
{
  auto i = m_entities.find(entityId);
  if (i != m_entities.end()) {
    auto& coords = i->second;

    m_cells[coords[1]][coords[0]].erase(entityId);
    m_entities.erase(i);
  }
}

bool SysGridImpl::hasEntity(EntityId entityId) const
{
  return m_entities.contains(entityId);
}

const Vec2i& SysGridImpl::entityPos(EntityId entityId) const
{
  try {
    return m_entities.at(entityId);
  }
  catch (const std::exception& e) { // TODO: More specific type
    EXCEPTION(STR("Entity " << entityId << " not in grid"));
  }
}

bool SysGridImpl::goTo(EntityId entityId, int x, int y)
{
  ASSERT(hasEntity(entityId), "Grid doesn't contain entity " << entityId);

  if (!isInRange(x, y)) {
    return false;
  }

  removeEntity(entityId);
  addEntity(entityId, x, y);

  return true;
}

bool SysGridImpl::tryMove(EntityId entityId, int dx, int dy)
{
  ASSERT(hasEntity(entityId), "Grid doesn't contain entity " << entityId);

  Vec2i dest = m_entities.at(entityId) + Vec2i{ dx, dy };

  if (!isInRange(dest[0], dest[1])) {
    return false;
  }

  auto entities = m_cells[dest[1]][dest[0]];

  removeEntity(entityId);
  addEntity(entityId, dest[0], dest[1]);

  if (!entities.empty()) {
    m_eventSystem.raiseEvent(EEntityEnter{entityId, dest, entities});
  }

  return true;
}

void SysGridImpl::addEntity(EntityId entityId, int x, int y)
{
  ASSERT(isInRange(x, y), "Grid coordinates (" << x << ", " << y << ") out of range");

  m_cells[y][x].insert(entityId);
  m_entities.insert({ entityId, { x, y }});
}

const fge::EntityIdSet& SysGridImpl::getEntities(int x, int y) const
{
  return m_cells[y][x];
}

fge::EntityIdSet SysGridImpl::getEntities(int minX, int minY, int maxX, int maxY) const
{
  fge::EntityIdSet entities;

  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      if (isInRange(x, y)) {
        auto& inCell = getEntities(x, y);
        entities.insert(inCell.begin(), inCell.end());
      }
    }
  }

  return entities;
}

bool SysGridImpl::hasEntityAt(EntityId entityId, int x, int y) const
{
  return m_cells[y][x].contains(entityId);
}

} // namespace

SysGridPtr createSysGrid(fge::EventSystem& eventSystem)
{
  return std::make_unique<SysGridImpl>(eventSystem);
}

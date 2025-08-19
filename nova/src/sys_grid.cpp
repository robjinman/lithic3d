#include "sys_grid.hpp"
#include "math.hpp"
#include "game_events.hpp"
#include <array>
#include <set>
#include <map>

namespace
{

const int GRID_W = 21;
const int GRID_H = 11;

class SysGridImpl : public SysGrid
{
  public:
    explicit SysGridImpl(EventSystem& eventSystem)
      : m_eventSystem(eventSystem)
    {}

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override {}
    void processEvent(const GameEvent& event) override {}

    void addEntity(EntityId entityId, int x, int y) override;
    const std::set<EntityId>& getEntities(int x, int y) const override;
    bool hasEntityAt(EntityId entityId, int x, int y) const override;
    bool tryMove(EntityId entityId, int dx, int dy) override;

  private:
    EventSystem& m_eventSystem;
    std::array<std::array<std::set<EntityId>, GRID_W>, GRID_H> m_cells;
    std::map<EntityId, Vec2i> m_entities;

    bool isInRange(int x, int y) const;
};

bool SysGridImpl::isInRange(int x, int y) const
{
  return inRange(x, 0, GRID_W - 1) && inRange(y, 0, GRID_H - 1);
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

bool SysGridImpl::tryMove(EntityId entityId, int dx, int dy)
{
  ASSERT(hasEntity(entityId), "Grid doesn't contain entity " << entityId);

  const Vec2i& coords = m_entities.at(entityId);
  Vec2i dest = coords + Vec2i{ dx, dy };

  if (!isInRange(dest[0], dest[1])) {
    return false;
  }

  removeEntity(entityId);
  addEntity(entityId, dest[0], dest[1]);

  m_eventSystem.fireEvent(EEntityStepOn{entityId, coords, dest, m_cells[dest[1]][dest[0]]});

  return true;
}

void SysGridImpl::addEntity(EntityId entityId, int x, int y)
{
  ASSERT(isInRange(x, y), "Grid coordinates (" << x << ", " << y << ") out of range");

  m_cells[y][x].insert(entityId);
  m_entities.insert({ entityId, { x, y }});
}

const std::set<EntityId>& SysGridImpl::getEntities(int x, int y) const
{
  return m_cells[y][x];
}

bool SysGridImpl::hasEntityAt(EntityId entityId, int x, int y) const
{
  return m_cells[y][x].contains(entityId);
}

} // namespace

SysGridPtr createSysGrid(EventSystem& eventSystem)
{
  return std::make_unique<SysGridImpl>(eventSystem);
}

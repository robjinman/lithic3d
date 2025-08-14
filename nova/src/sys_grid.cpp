#include "sys_grid.hpp"
#include <array>
#include <set>

namespace
{

const uint32_t GRID_W = 100;
const uint32_t GRID_H = 80;

class SysGridImpl : public SysGrid
{
  public:
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update() override;
    void processEvent(const GameEvent& event) override;

    void addEntity(EntityId entityId, uint32_t x, uint32_t y) override;
    const std::set<EntityId>& getEntities(uint32_t x, uint32_t y) const override;
    bool hasEntityAt(EntityId entityId, uint32_t x, uint32_t y) const override;

  private:
    std::array<std::array<std::set<EntityId>, GRID_W>, GRID_H> m_cells;
};

void SysGridImpl::removeEntity(EntityId entityId)
{

}

bool SysGridImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysGridImpl::update()
{

}

void SysGridImpl::processEvent(const GameEvent& event)
{

}

void SysGridImpl::addEntity(EntityId entityId, uint32_t x, uint32_t y)
{

}

const std::set<EntityId>& SysGridImpl::getEntities(uint32_t x, uint32_t y) const
{
  return m_cells[y][x];
}

bool SysGridImpl::hasEntityAt(EntityId entityId, uint32_t x, uint32_t y) const
{
  // TODO
  return false;
}

}

SysGridPtr createSysGrid()
{
  return std::make_unique<SysGridImpl>();
}

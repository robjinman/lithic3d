#include "world_grid.hpp"
#include <array>
#include <set>

namespace
{

const uint32_t GRID_W = 100;
const uint32_t GRID_H = 80;

class WorldGridImpl : public WorldGrid
{
  public:
    void addEntity(uint32_t x, uint32_t y, EntityId entityId) override;
    void removeEntity(uint32_t x, uint32_t y, EntityId entityId) override;
    const std::set<EntityId>& getEntities(uint32_t x, uint32_t y) const override;
    bool hasEntityAt(uint32_t x, uint32_t y, EntityId entityId) const override;

  private:
    std::array<std::array<std::set<EntityId>, GRID_W>, GRID_H> m_cells;
};

}

WorldGridPtr createWorldGrid()
{
  return std::make_unique<WorldGrid>();
}

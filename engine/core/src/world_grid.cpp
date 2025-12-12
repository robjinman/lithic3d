#include "lithic3d/world_grid.hpp"
#include "lithic3d/world_loader.hpp"
#include "lithic3d/entity_id.hpp"
#include "lithic3d/events.hpp"
#include <map>
#include <set>

namespace lithic3d
{
namespace
{

bool isInRange(const Vec2i& p, const Vec2i& min, const Vec2i& max)
{
  return p[0] >= min[0] && p[0] <= max[0] && p[1] >= min[1] && p[1] <= max[1];
}

struct CellSlicePendingErasure
{
  Vec3i coord;
  uint32_t timeToLive = 5;
};

class WorldGridImpl : public WorldGrid
{
  public:
    WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader,
      EventSystem& eventSystem);

    void update(const Vec3f& cameraPos) override;

  private:
    WorldTraversalOptions m_options;
    WorldLoader& m_worldLoader;
    EventSystem& m_eventSystem;
    std::vector<ResourceHandle> m_pendingCellSlices;
    // x, y, slice
    std::map<Vec3i, ResourceHandle> m_cellSlices;
    std::vector<CellSlicePendingErasure> m_cellSlicesPendingErasure;
    std::map<Vec3i, std::set<EntityId>> m_entities;
    Vec2i m_lastGridPos = { -1, -1 };

    void doUpdate(int cellX, int cellY);
};

WorldGridImpl::WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  EventSystem& eventSystem)
  : m_options(options)
  , m_worldLoader(worldLoader)
  , m_eventSystem(eventSystem)
{
}

void WorldGridImpl::update(const Vec3f& cameraPos)
{
  float cellW = m_worldLoader.worldInfo().cellWidth;
  float cellH = m_worldLoader.worldInfo().cellHeight;

  auto cellX = static_cast<int>(cameraPos[0] / cellW);
  auto cellY = static_cast<int>(cameraPos[2] / cellH);

  if (m_lastGridPos != Vec2i{ cellX, cellY }) {
    doUpdate(cellX, cellY);
    m_lastGridPos = { cellX, cellY };
  }

  for (auto i = m_pendingCellSlices.begin(); i != m_pendingCellSlices.end();) {
    if (i->ready()) {
      m_worldLoader.createEntities(i->id());
      i = m_pendingCellSlices.erase(i);
    }
    else {
      ++i;
    }
  }

  for (auto i = m_cellSlicesPendingErasure.begin(); i != m_cellSlicesPendingErasure.end();) {
    if (--i->timeToLive == 0) {
      m_cellSlices.erase(i->coord);
      i = m_cellSlicesPendingErasure.erase(i);
    }
    else {
      ++i;
    }
  }
}

void WorldGridImpl::doUpdate(int cellX, int cellY)
{
  int gridW = m_worldLoader.worldInfo().gridWidth;
  int gridH = m_worldLoader.worldInfo().gridHeight;

  for (auto sliceIdx = 0; sliceIdx < NUM_WORLD_SLICES; ++sliceIdx) {
    int loadDistance = m_options.sliceLoadDistances[sliceIdx];

    Vec2i minForCurrentPos{
      std::max(cellX - loadDistance, 0),
      std::max(cellY - loadDistance, 0)
    };

    Vec2i maxForCurrentPos{
      std::min(cellX + loadDistance, gridW - 1),
      std::min(cellY + loadDistance, gridH - 1)
    };

    Vec2i minForLastPos{
      std::max(m_lastGridPos[0] - loadDistance, 0),
      std::max(m_lastGridPos[1] - loadDistance, 0)
    };

    Vec2i maxForLastPos{
      std::min(m_lastGridPos[0] + loadDistance, gridW - 1),
      std::min(m_lastGridPos[1] + loadDistance, gridH - 1)
    };

    int minX = std::min(minForCurrentPos[0], minForLastPos[0]);
    int maxX = std::max(maxForCurrentPos[0], maxForLastPos[0]);
    int minY = std::min(minForCurrentPos[1], minForLastPos[1]);
    int maxY = std::max(maxForCurrentPos[1], maxForLastPos[1]);

    for (int x = minX; x <= maxX; ++x) {
      for (int y = minY; y <= maxY; ++y) {
        Vec3i coord = { x, y, sliceIdx };

        if (isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          !isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          for (auto entityId : m_entities.at(coord)) {
            m_eventSystem.raiseEvent(ERequestDeletion{entityId});
          }

          CellSlicePendingErasure pendingErasure;
          pendingErasure.coord = coord;
          m_cellSlicesPendingErasure.push_back(pendingErasure);
        }
        else if (!isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          auto handle = m_worldLoader.loadCellSliceAsync(x, y, sliceIdx);
          m_cellSlices.insert({ coord, handle });

          m_pendingCellSlices.push_back(handle);
        }
      }
    }
  }
}

} // namespace

WorldGridPtr createWorldGrid(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  EventSystem& eventSystem)
{
  return std::make_unique<WorldGridImpl>(options, worldLoader, eventSystem);
}

} // namespace lithic3d

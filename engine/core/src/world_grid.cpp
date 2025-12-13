#include "lithic3d/world_grid.hpp"
#include "lithic3d/world_loader.hpp"
#include "lithic3d/entity_id.hpp"
#include "lithic3d/events.hpp"
#include "lithic3d/logger.hpp"
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
  std::tuple<uint32_t, uint32_t, uint32_t> coord;
  uint32_t timeToLive = 5;
};

struct PendingCellSlice
{
  std::tuple<uint32_t, uint32_t, uint32_t> coord;
  ResourceHandle handle;
};

class WorldGridImpl : public WorldGrid
{
  public:
    WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader,
      EventSystem& eventSystem, Logger& logger);

    void update(const Vec3f& cameraPos) override;

  private:
    Logger& m_logger;
    WorldTraversalOptions m_options;
    WorldLoader& m_worldLoader;
    EventSystem& m_eventSystem;
    std::vector<PendingCellSlice> m_pendingCellSlices;
    // x, y, slice
    std::map<std::tuple<uint32_t, uint32_t, uint32_t>, ResourceHandle> m_cellSlices;
    std::vector<CellSlicePendingErasure> m_cellSlicesPendingErasure;
    std::map<std::tuple<uint32_t, uint32_t, uint32_t>, std::set<EntityId>> m_entities;
    Vec2i m_lastGridPos = { -1, -1 };

    void doUpdate(int cellX, int cellY);
};

WorldGridImpl::WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_options(options)
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
    //m_logger.debug(STR("Waiting for resource " << i->id()));

    if (i->handle.ready()) {
      m_logger.debug(STR("Resource " << i->handle.id() << " ready!"));

      auto entities = m_worldLoader.createEntities(i->handle.id());

      for (auto entityId : entities) {
        m_entities[i->coord].insert(entityId);
      }

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
  m_logger.debug(STR("Camera is in cell " << cellX << ", " << cellY));

  bool initialLoad = m_lastGridPos == Vec2i{ -1, -1 };

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

    int minX = minForCurrentPos[0];
    int maxX = maxForCurrentPos[0];
    int minY = minForCurrentPos[1];
    int maxY = maxForCurrentPos[1];

    if (!initialLoad) {
      minX = std::min(minForCurrentPos[0], minForLastPos[0]);
      maxX = std::max(maxForCurrentPos[0], maxForLastPos[0]);
      minY = std::min(minForCurrentPos[1], minForLastPos[1]);
      maxY = std::max(maxForCurrentPos[1], maxForLastPos[1]);
    }

    for (int x = minX; x <= maxX; ++x) {
      for (int y = minY; y <= maxY; ++y) {
        std::tuple<uint32_t, uint32_t, uint32_t> coord = { x, y, sliceIdx };

        if (!initialLoad && isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          !isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          m_logger.info(STR("Unloading cell " << x << ", " << y << ", slice " << sliceIdx));

          auto i = m_entities.find(coord);
          if (i != m_entities.end()) {
            for (auto entityId : m_entities.at(coord)) {
              m_eventSystem.raiseEvent(ERequestDeletion{entityId});
            }
          }

          CellSlicePendingErasure pendingErasure;
          pendingErasure.coord = coord;
          m_cellSlicesPendingErasure.push_back(pendingErasure);
        }
        else if ((initialLoad || !isInRange({ x, y }, minForLastPos, maxForLastPos)) &&
          isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          m_logger.info(STR("Loading cell " << x << ", " << y << ", slice " << sliceIdx));

          auto handle = m_worldLoader.loadCellSliceAsync(x, y, sliceIdx);
          m_cellSlices.insert({ coord, handle });

          m_pendingCellSlices.push_back(PendingCellSlice{
            .coord = coord,
            .handle = handle
          });
        }
      }
    }
  }
}

} // namespace

WorldGridPtr createWorldGrid(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<WorldGridImpl>(options, worldLoader, eventSystem, logger);
}

} // namespace lithic3d

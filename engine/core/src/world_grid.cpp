#include "lithic3d/world_grid.hpp"
#include "lithic3d/world_loader.hpp"
#include "lithic3d/entity_id.hpp"
#include "lithic3d/events.hpp"
#include "lithic3d/ecs.hpp"
#include "lithic3d/logger.hpp"
#include <map>
#include <set>
#include <queue>
#include <cassert>

namespace lithic3d
{
namespace
{

const Vec2i NULL_POS{
  std::numeric_limits<int>::max(),
  std::numeric_limits<int>::max()
};

bool isInRange(const Vec2i& p, const Vec2i& min, const Vec2i& max)
{
  return p[0] >= min[0] && p[0] <= max[0] && p[1] >= min[1] && p[1] <= max[1];
}

// x, y, slice
using SliceCoords = std::array<int, 3>;

enum class SliceState
{
  Unloaded,               // Fully unloaded
  LoadInitiated,          // Resources being created on resource thread
  PendingResourceUnload,  // Entities deleted, waiting a few frames before unloading resources
  Loaded                  // Fully loaded
};

class CellSlice
{
  public:
    CellSlice(const SliceCoords& coords, WorldLoader& worldLoader, Ecs& ecs, Logger& logger);

    void load();
    void unload();
    void update();
    void wait();

    bool isUnloaded() const;

  private:
    Logger& m_logger;
    WorldLoader& m_worldLoader;
    Ecs& m_ecs;
    SliceCoords m_coords;
    ResourceHandle m_handle;
    SliceState m_state = SliceState::Unloaded;
    SliceState m_desiredState = SliceState::Unloaded; // One of Loaded or Unloaded
    std::set<EntityId> m_entities;
    uint32_t m_resourcesTimeToLive = 0;

    void stateUnloadedUpdate();
    void stateLoadInitiatedUpdate();
    void statePendingResourceUnloadUpdate();
    void stateLoadedUpdate();
};

CellSlice::CellSlice(const SliceCoords& coords, WorldLoader& worldLoader, Ecs& ecs, Logger& logger)
  : m_logger(logger)
  , m_worldLoader(worldLoader)
  , m_ecs(ecs)
  , m_coords(coords)
{}

void CellSlice::wait()
{
  if (m_state == SliceState::LoadInitiated) {
    m_handle.wait();
  }
}

void CellSlice::load()
{
  m_desiredState = SliceState::Loaded;
}

void CellSlice::unload()
{
  m_desiredState = SliceState::Unloaded;
}

void CellSlice::update()
{
  switch (m_state) {
    case SliceState::Unloaded: stateUnloadedUpdate(); break;
    case SliceState::LoadInitiated: stateLoadInitiatedUpdate(); break;
    case SliceState::PendingResourceUnload: statePendingResourceUnloadUpdate(); break;
    case SliceState::Loaded: stateLoadedUpdate(); break;
  }
}

void CellSlice::stateUnloadedUpdate()
{
  if (m_desiredState == SliceState::Loaded) {
    m_logger.info(STR("Loading cell " << m_coords[0] << ", " << m_coords[1]
      << ", slice " << m_coords[2]));

    m_handle = m_worldLoader.loadCellSliceAsync(m_coords[0], m_coords[1], m_coords[2]);

    m_state = SliceState::LoadInitiated;
  }
  else {
    assert(m_desiredState == SliceState::Unloaded);
    // Nothing to do
  }
}

void CellSlice::stateLoadInitiatedUpdate()
{
  if (m_desiredState == SliceState::Loaded) {
    if (m_handle.ready()) {
      auto entities = m_worldLoader.createEntities(m_handle.id());

      for (auto entityId : entities) {
        m_entities.insert(entityId);
      }

      m_state = SliceState::Loaded;
    }
  }
  else {
    assert(m_desiredState == SliceState::Unloaded);
    m_handle.reset();
    m_state = SliceState::Unloaded;
  }
}

void CellSlice::stateLoadedUpdate()
{
  if (m_desiredState == SliceState::Loaded) {
    // Nothing to do
  }
  else {
    assert(m_desiredState == SliceState::Unloaded);

    for (auto entityId : m_entities) {
      m_ecs.removeEntity(entityId);
    }

    m_resourcesTimeToLive = 10;
    m_state = SliceState::PendingResourceUnload;
  }
}

void CellSlice::statePendingResourceUnloadUpdate()
{
  if (m_desiredState == SliceState::Loaded) {
    auto entities = m_worldLoader.createEntities(m_handle.id());

    for (auto entityId : entities) {
      m_entities.insert(entityId);
    }

    m_state = SliceState::Loaded;
  }
  else {
    assert(m_desiredState == SliceState::Unloaded);
    if (--m_resourcesTimeToLive == 0) {
      m_handle.reset();
      m_state = SliceState::Unloaded;
    }
  }
}

bool CellSlice::isUnloaded() const
{
  return m_state == SliceState::Unloaded && m_desiredState == SliceState::Unloaded;
}

class WorldGridImpl : public WorldGrid
{
  public:
    WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader, Ecs& ecs,
      Logger& logger);

    void update(const Vec3f& cameraPos) override;
    void wait() override;

  private:
    Logger& m_logger;
    WorldTraversalOptions m_options;
    WorldLoader& m_worldLoader;
    Ecs& m_ecs;
    Vec2i m_lastGridPos = NULL_POS;
    std::map<SliceCoords, CellSlice> m_slices;

    void onCellEntry(int cellX, int cellY);
};

WorldGridImpl::WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  Ecs& ecs, Logger& logger)
  : m_logger(logger)
  , m_options(options)
  , m_worldLoader(worldLoader)
  , m_ecs(ecs)
{
}

void WorldGridImpl::wait()
{
  for (auto& slice : m_slices) {
    slice.second.wait();
  }
}

void WorldGridImpl::update(const Vec3f& cameraPos)
{
  float cellW = m_worldLoader.worldInfo().cellWidth;
  float cellH = m_worldLoader.worldInfo().cellHeight;

  auto cellX = static_cast<int>(cameraPos[0] / cellW);
  auto cellY = static_cast<int>(cameraPos[2] / cellH);

  if (m_lastGridPos != Vec2i{ cellX, cellY }) {
    onCellEntry(cellX, cellY);
    m_lastGridPos = { cellX, cellY };
  }

  for (auto i = m_slices.begin(); i != m_slices.end();) {
    i->second.update();

    if (i->second.isUnloaded()) {
      i = m_slices.erase(i);
    }
    else {
      ++i;
    }
  }
}

void WorldGridImpl::onCellEntry(int cellX, int cellY)
{
  m_logger.debug(STR("Camera is in cell " << cellX << ", " << cellY));

  bool initialLoad = m_lastGridPos == NULL_POS;

  int gridW = m_worldLoader.worldInfo().gridWidth;
  int gridH = m_worldLoader.worldInfo().gridHeight;

  for (int sliceIdx = 0; sliceIdx < NUM_WORLD_SLICES; ++sliceIdx) {
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
        SliceCoords coords = { x, y, sliceIdx };

        if (!initialLoad && isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          !isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          auto i = m_slices.find(coords);
          ASSERT(i != m_slices.end(), "Error unloading cell slice - slice isn't loaded");

          i->second.unload();
        }
        else if ((initialLoad || !isInRange({ x, y }, minForLastPos, maxForLastPos)) &&
          isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          auto i = m_slices.find(coords);
          if (i == m_slices.end()) {
            auto [ j, result ] = m_slices.insert({
              coords,
              CellSlice{coords, m_worldLoader, m_ecs, m_logger}
            });

            assert(result);

            j->second.load();
          }
          else {
            i->second.load();
          }
        }
      }
    }
  }
}

} // namespace

WorldGridPtr createWorldGrid(const WorldTraversalOptions& options, WorldLoader& worldLoader,
  Ecs& ecs, Logger& logger)
{
  return std::make_unique<WorldGridImpl>(options, worldLoader, ecs, logger);
}

} // namespace lithic3d

#include "lithic3d/world_grid.hpp"
#include "lithic3d/world_loader.hpp"
#include <map>

namespace lithic3d
{
namespace
{

bool isInRange(const Vec2i& p, const Vec2i& min, const Vec2i& max)
{
  return p[0] >= min[0] && p[0] <= max[0] && p[1] >= min[1] && p[1] <= max[1];
}

// x, y, slice
using Grid = std::map<std::tuple<uint32_t, uint32_t, uint32_t>, ResourceHandle>;

struct World
{
  Grid grid;
};

class WorldGridImpl : public WorldGrid
{
  public:
    WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader);

    void update(const Vec3f& cameraPos) override;

  private:
    WorldLoader& m_worldLoader;
    WorldTraversalOptions m_options;
    Grid m_grid;
    Vec2i m_lastGridPos = { -1, -1 };

    void doUpdate(int cellX, int cellY);
};

WorldGridImpl::WorldGridImpl(const WorldTraversalOptions& options, WorldLoader& worldLoader)
  : m_worldLoader(worldLoader)
  , m_options(options)
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
        if (isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          !isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          m_grid.erase({ x, y, sliceIdx });
        }
        else if (!isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          m_grid.insert({{ x, y, sliceIdx }, m_worldLoader.loadCellSliceAsync(x, y, sliceIdx)});
        }
      }
    }
  }
}

} // namespace

WorldGridPtr createWorldGrid(const WorldTraversalOptions& options, const WorldLoader& worldLoader)
{
  return std::make_unique<WorldGridImpl>(options, worldLoader);
}

}

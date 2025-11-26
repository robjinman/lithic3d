#include "lithic3d/world_traversal.hpp"
#include "lithic3d/resource_manager.hpp"
#include "lithic3d/world_loader.hpp"

namespace lithic3d
{
namespace
{

bool isInRange(const Vec2i& p, const Vec2i& min, const Vec2i& max)
{
  return p[0] >= min[0] && p[0] <= max[0] && p[1] >= min[1] && p[1] <= max[1];
}

class WorldTraversalImpl : public WorldTraversal
{
  public:
    WorldTraversalImpl(const WorldTraversalOptions& options, const WorldGrid& worldGrid,
      ResourceManager& resourceManager);

    void update(const Vec3f& cameraPos) override;

  private:
    ResourceManager& m_resourceManager;
    WorldTraversalOptions m_options;
    WorldGrid m_worldGrid;
    Vec2i m_lastGridPos = { -1, -1 };

    void doUpdate(int cellX, int cellY);
};

WorldTraversalImpl::WorldTraversalImpl(const WorldTraversalOptions& options,
  const WorldGrid& worldGrid, ResourceManager& resourceManager)
  : m_resourceManager(resourceManager)
  , m_options(options)
  , m_worldGrid(worldGrid)
{
}

void WorldTraversalImpl::update(const Vec3f& cameraPos)
{
  float cellW = m_worldGrid.sizeMetres[0] / m_worldGrid.sizeCells[0];
  float cellH = m_worldGrid.sizeMetres[1] / m_worldGrid.sizeCells[1];

  auto cellX = static_cast<int>(cameraPos[0] / cellW);
  auto cellY = static_cast<int>(cameraPos[2] / cellH);

  if (m_lastGridPos != Vec2i{ cellX, cellY }) {
    doUpdate(cellX, cellY);
    m_lastGridPos = { cellX, cellY };
  }
}

void WorldTraversalImpl::doUpdate(int cellX, int cellY)
{
  int gridW = m_worldGrid.sizeCells[0];
  int gridH = m_worldGrid.sizeCells[1];

  size_t numLayers = m_options.layerOptions.size();

  std::vector<ResourceId> toLoad;
  std::vector<ResourceId> toUnload;

  for (auto layerIdx = 0; layerIdx < numLayers; ++layerIdx) {
    auto& layerOpts = m_options.layerOptions[layerIdx];
    int loadDistance = static_cast<int>(layerOpts.loadDistance);

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
        ResourceId layerResource = m_worldGrid.cells[y * gridW + x].layers[layerIdx];

        if (isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          !isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          toUnload.push_back(layerResource);
        }
        else if (!isInRange({ x, y }, minForLastPos, maxForLastPos) &&
          isInRange({ x, y }, minForCurrentPos, maxForCurrentPos)) {

          toLoad.push_back(layerResource);
        }
      }
    }
  }

  m_resourceManager.unloadResources(toUnload);
  m_resourceManager.loadResources(toLoad);
}

} // namespace

WorldTraversalPtr createWorldTraversal(const WorldTraversalOptions& options, const WorldGrid& grid,
  ResourceManager& resourceManager)
{
  return std::make_unique<WorldTraversalImpl>(options, grid, resourceManager);
}

}

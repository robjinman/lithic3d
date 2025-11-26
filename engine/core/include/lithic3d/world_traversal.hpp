#pragma once

#include "math.hpp"
#include <memory>

namespace lithic3d
{

struct WorldTraversalLayerOptions
{
  // A value of 0 means the layer is only loaded when the camera enters the cell
  // A value of 1 means the layer is loaded when the camera enters an adjacent cell
  // etc.
  uint32_t loadDistance = 0;
};

struct WorldTraversalOptions
{
  std::vector<WorldTraversalLayerOptions> layerOptions;
};

class WorldTraversal
{
  public:
    virtual void update(const Vec3f& cameraPos) = 0;

    virtual ~WorldTraversal() = default;
};

using WorldTraversalPtr = std::unique_ptr<WorldTraversal>;

class WorldGrid;
class ResourceManager;

WorldTraversalPtr createWorldTraversal(const WorldTraversalOptions& options, const WorldGrid& grid,
  ResourceManager& resourceManager);

}

#pragma once

#include "math.hpp"
#include <memory>
#include <array>

namespace lithic3d
{

constexpr uint32_t NUM_WORLD_SLICES = 6;

struct WorldTraversalOptions
{
  // A value of 0 means the slice is only loaded when the camera enters the cell
  // A value of 1 means the slice is loaded when the camera enters an adjacent cell
  // etc.
  uint32_t loadDistance = 0;
  std::array<uint32_t, NUM_WORLD_SLICES> sliceLoadDistances;
};

class WorldGrid
{
  public:
    virtual void update(const Vec3f& cameraPos) = 0;

    virtual ~WorldGrid() = default;
};

using WorldGridPtr = std::unique_ptr<WorldGrid>;

class WorldLoader;

WorldGridPtr createWorldGrid(const WorldTraversalOptions& options, WorldLoader& worldLoader);

}

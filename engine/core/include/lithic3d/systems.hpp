#pragma once

#include "ecs.hpp"

namespace lithic3d
{

// Systems are updated in order
namespace Systems
{
  enum : uint32_t {
    Collision,
    Animation2d,
    Spatial,
    Ui,
    Behaviour,
    Render3d,
    Render2d,
    // ...

    NUMBER_OF_SYSTEMS
  };

  const uint32_t LAST_SYSTEM_ID = NUMBER_OF_SYSTEMS - 1;
}

} // namespace lithic3d

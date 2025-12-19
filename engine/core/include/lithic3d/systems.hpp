#pragma once

#include "ecs.hpp"

namespace lithic3d
{

// Systems are updated in order
enum : uint32_t {
  COLLISION_SYSTEM,
  ANIMATION_2D_SYSTEM,
  SPATIAL_SYSTEM,
  UI_SYSTEM,
  BEHAVIOUR_SYSTEM,
  RENDER_3D_SYSTEM,
  RENDER_2D_SYSTEM,
  // ...

  NUMBER_OF_SYSTEMS
};

const uint32_t LAST_SYSTEM_ID = NUMBER_OF_SYSTEMS - 1;

} // namespace lithic3d

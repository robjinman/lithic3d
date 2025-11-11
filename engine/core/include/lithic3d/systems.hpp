#pragma once

#include "ecs.hpp"

namespace lithic3d
{

// Systems are updated in order
const uint32_t ANIMATION_2D_SYSTEM = 0;
const uint32_t SPATIAL_SYSTEM = 1;
const uint32_t UI_SYSTEM = 2;
const uint32_t BEHAVIOUR_SYSTEM = 3;
const uint32_t GRID_SYSTEM = 4;
const uint32_t RENDER_3D_SYSTEM = 5;
const uint32_t RENDER_2D_SYSTEM = 6;
const uint32_t LAST_SYSTEM_ID = RENDER_2D_SYSTEM;

} // namespace lithic3d

#pragma once

#include "ecs.hpp"

enum ComponentTypeId : ComponentType
{
  Animation   = 1 << 0,
  Behaviour   = 1 << 1,
  Grid        = 1 << 2,
  Render      = 1 << 3,
  Ui          = 1 << 4
};

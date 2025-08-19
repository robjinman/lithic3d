#pragma once

#include "component_store.hpp"

enum ComponentTypeId : ComponentType
{
  CAnimationTypeId   = 1 << 0,
  CBehaviourTypeId   = 1 << 1,
  CGridTypeId        = 1 << 2,
  CRenderTypeId      = 1 << 3,
  CUiTypeId          = 1 << 4
};

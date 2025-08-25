#pragma once

#include "component_store.hpp"

enum ComponentTypeId : ComponentType
{
  CAnimationTypeId   = 1u << 0,
  CBehaviourTypeId   = 1u << 1,
  CGridTypeId        = 1u << 2,
  CRenderTypeId      = 1u << 3,
  CUiTypeId          = 1u << 4
};

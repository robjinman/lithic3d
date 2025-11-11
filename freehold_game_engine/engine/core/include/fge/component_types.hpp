#pragma once

#include "component_store.hpp"

namespace fge
{

enum ComponentTypeId : ComponentType
{
  CLocalTransformTypeId   = 1u << 0,
  CGlobalTransformTypeId  = 1u << 1,
  CSpatialFlagsTypeId     = 1u << 2,
  CRender2dTypeId         = 1u << 3,
  CSpriteTypeId           = 1u << 4,
  CDynamicTextTypeId      = 1u << 5,
  CQuadId                 = 1u << 6,
  CUiTypeId               = 1u << 7,
  CTextboxTypeId          = 1u << 8
};

} // namespace fge

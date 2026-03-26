#pragma once

#include "component_store.hpp"

namespace lithic3d
{

enum : ComponentTypeId
{
  CLocalTransformTypeId       = 1u << 0,
  CGlobalTransformTypeId      = 1u << 1,
  CSpatialFlagsTypeId         = 1u << 2,
  CRender2dTypeId             = 1u << 3,
  CSpriteTypeId               = 1u << 4,
  CDynamicTextTypeId          = 1u << 5,
  CQuadTypeId                 = 1u << 6,
  CUiTypeId                   = 1u << 7,
  CTextboxTypeId              = 1u << 8,
  CBoundingBoxTypeId          = 1u << 9,
  CMaterial2dTypeId           = 1u << 10,
  CMesh2dTypeId               = 1u << 11,
  CCollisionTypeId            = 1u << 12,
  CCollisionTerrainTypeId     = 1u << 13,
  CCollisionDynamicTypeId     = 1u << 14,
  CCollisionBoxTypeId         = 1u << 15,
  CCollisionCapsuleTypeId     = 1u << 16,
  //CCollisionPolyhedronTypeId  = 1u << 17
};

} // namespace lithic3d

#pragma once

#include "ecs.hpp"
#include "component_types.hpp"

// TODO: Better naming convention
struct CSpatial
{
  Mat4x4f transform = identityMatrix<float_t, 4>();
  EntityId parent = NULL_ENTITY;
  bool isActive = true;
};

struct CLocalTransform
{
  Mat4x4f transform = identityMatrix<float_t, 4>();

  static constexpr ComponentType TypeId = CLocalTransformTypeId;
};

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<float_t, 4>();

  static constexpr ComponentType TypeId = CGlobalTransformTypeId;
};

struct CSpatialFlags
{
  bool active = true;

  static constexpr ComponentType TypeId = CSpatialFlagsTypeId;
};

class SysSpatial : public System
{
  public:
    virtual EntityId root() const = 0;
    virtual void addEntity(EntityId entityId, const CSpatial& data) = 0;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(ComponentStore& componentStore);

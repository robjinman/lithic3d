#pragma once

#include "ecs.hpp"

struct CSpatial
{
  Mat4x4f transform;
  EntityId parent;
  bool isActive; // Replace with bitset if we need multiple flags
};

class SysSpatial : public System
{
  public:
    virtual void addEntity(EntityId entityId, const CSpatial& data) = 0;

    // TODO: Methods to allow modification of hierarchy
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial();

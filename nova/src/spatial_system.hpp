#pragma once

#include "system.hpp"
#include "math.hpp"

struct CSpatial : public Component
{
  CSpatial(EntityId entityId)
    : Component(entityId) {}

  Mat4x4f transform = identityMatrix<float_t, 4>();
};

class SpatialSystem : public System
{
  public:
    CSpatial& getComponent(EntityId entityId) override = 0;
    const CSpatial& getComponent(EntityId entityId) const override = 0;

    virtual ~SpatialSystem() {}
};

using SpatialSystemPtr = std::unique_ptr<SpatialSystem>;

class Logger;

SpatialSystemPtr createSpatialSystem(Logger& logger);

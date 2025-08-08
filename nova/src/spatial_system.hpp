#pragma once

#include "system.hpp"

class CSpatial : public Component
{
  CSpatial(EntityId entityId)
    : Component(entityId) {}
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

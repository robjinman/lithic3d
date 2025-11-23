#pragma once

#include "systems.hpp"
#include "ecs.hpp"

namespace lithic3d
{

class SysCollision : public System
{
  public:
};

using SysCollisionPtr = std::unique_ptr<SysCollision>;

SysCollisionPtr createSysCollision();

}

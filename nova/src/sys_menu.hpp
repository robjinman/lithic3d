#pragma once

#include "ecs.hpp"

class SysMenu : public System
{
  public:
    // TODO
};

using SysMenuPtr = std::unique_ptr<SysMenu>;

SysMenuPtr createSysMenu();

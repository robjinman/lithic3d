#pragma once

#include "system.hpp"

class UiEvent : public Event
{
};

class SysUi : public System
{
  public:
    // TODO
};

using SysUiPtr = std::unique_ptr<SysUi>;

SysUiPtr createSysUi();

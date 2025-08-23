#pragma once

#include "sys_behaviour.hpp"

class SysRender;
class EventSystem;

CBehaviourPtr createBNumericTile(SysRender& sysRender, EventSystem& eventSystem, EntityId entityId,
  const Vec2i& pos, int value);

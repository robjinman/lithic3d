#pragma once

#include <memory>
#include <functional>

struct MobileControlsCallbacks
{
  std::function<void()> onLeftButtonPress;
  std::function<void()> onLeftButtonRelease;
  std::function<void()> onRightButtonPress;
  std::function<void()> onRightButtonRelease;
  std::function<void()> onUpButtonPress;
  std::function<void()> onUpButtonRelease;
  std::function<void()> onDownButtonPress;
  std::function<void()> onDownButtonRelease;
  std::function<void()> onActionButtonPress;
  std::function<void()> onActionButtonRelease;
  std::function<void()> onEscapeButtonPress;
  std::function<void()> onEscapeButtonRelease;
};

class MobileControls
{
  public:
    virtual void hide() = 0;
    virtual void show() = 0;

    virtual ~MobileControls() = default;
};

using MobileControlsPtr = std::unique_ptr<MobileControls>;

class Ecs;
class EventSystem;

MobileControlsPtr createMobileControls(Ecs& ecs, EventSystem& eventSystem,
  const MobileControlsCallbacks& callbacks, float screenAspect, float gameAreaAspect);

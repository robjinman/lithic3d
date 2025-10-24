#pragma once

#include <fge/window_delegate.hpp>
#include <fge/input.hpp>
#include <memory>

class Application
{
  public:
    virtual bool update() = 0;
    virtual void onViewResize(float w, float h) = 0;
    virtual void onTouchBegin(float x, float y) = 0;
    virtual void onTouchMove(float x, float y) = 0;
    virtual void onTouchEnd(float x, float y) = 0;
    virtual void onButtonDown(fge::GamepadButton button) = 0;
    virtual void onButtonUp(fge::GamepadButton button) = 0;
    virtual void onLeftStickMove(float x, float y) = 0;
    virtual void onRightStickMove(float x, float y) = 0;
    virtual void hideMobileControls() = 0;

    virtual ~Application() {}
};

using ApplicationPtr = std::unique_ptr<Application>;

ApplicationPtr createApplication(const char* bundlePath, const char* appSupportPath,
  fge::WindowDelegatePtr windowDelegate);

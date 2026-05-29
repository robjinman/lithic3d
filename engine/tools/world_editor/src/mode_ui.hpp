#pragma once

#include <lithic3d/input.hpp>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <memory>

class ModeUi
{
  public:
    virtual void activate() = 0;
    virtual void deactivate() = 0;

    virtual void update() = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;

    virtual void saveChanges() = 0;

    virtual ~ModeUi() = default;
};

using ModeUiPtr = std::unique_ptr<ModeUi>;

class EditorCore;

struct Panels
{
  wxPanel* leftSidebar;
  wxPanel* rightSidebar;
};

ModeUiPtr createSceneEditModeUi(const Panels& panels, EditorCore& editorCore);
ModeUiPtr createEntityEditModeUi(const Panels& panels, EditorCore& editorCore);

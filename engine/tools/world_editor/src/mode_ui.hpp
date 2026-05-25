#pragma once

#include <lithic3d/input.hpp>
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

ModeUiPtr createSceneEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore);
ModeUiPtr createPrefabEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore);

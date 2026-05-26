#pragma once

#include <lithic3d/input.hpp>
#include <memory>

class PrefabEditMode
{
  public:
    virtual void activate() = 0;
    virtual void deactivate() = 0;
    virtual void update() = 0;
    virtual void saveChanges() = 0;
    virtual void setActivePrefab(const std::string& name) = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;

    virtual ~PrefabEditMode() = default;
};

using PrefabEditModePtr = std::unique_ptr<PrefabEditMode>;

class EditorCore;

PrefabEditModePtr createPrefabEditMode(EditorCore& core);

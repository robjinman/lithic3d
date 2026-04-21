#pragma once

#include <lithic3d/math.hpp>
#include <lithic3d/input.hpp>
#include <memory>
#include <filesystem>
#include <vector>

class WorldEditor
{
  public:
    virtual std::vector<std::string> listPrefabs() const = 0;
    virtual void instantiatePrefabAtCursor(const std::string& name) = 0;

    virtual void update() = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;
    virtual void onCanvasResize(uint32_t w, uint32_t h) = 0;

    virtual ~WorldEditor() = default;
};

using WorldEditorPtr = std::unique_ptr<WorldEditor>;

namespace lithic3d { class WindowDelegate; }

WorldEditorPtr createWorldEditor(const std::filesystem::path& projectRoot,
  lithic3d::WindowDelegate& windowDelegate);

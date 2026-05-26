#pragma once

#include "event_emitter.hpp"
#include <lithic3d/math.hpp>
#include <lithic3d/input.hpp>
#include <lithic3d/game.hpp>
#include <memory>
#include <filesystem>
#include <vector>

namespace lithic3d { class Engine; }

// Manages the current state of the editor.
class EditorCore
{
  public:
    enum class Event : EventId
    {
      CursorMove
    };

    virtual const lithic3d::GameConfig& config() const = 0;
    virtual lithic3d::Engine& engine() const = 0;

    virtual float getCursorDistance() const = 0;
    virtual lithic3d::Vec3f getCursorRotation() = 0;
    virtual const lithic3d::Vec3f& getCursorScale() const = 0;
    virtual lithic3d::Mat4x4f getCursorTransform() const = 0;

    virtual void setCursorDistance(float distance) = 0;
    virtual void setCursorRotation(const lithic3d::Vec3f& ori) = 0;
    virtual void setCursorScale(const lithic3d::Vec3f& scale) = 0;
    virtual void setCursorRotationScale(const lithic3d::Mat3x3f& m) = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;

    virtual const lithic3d::InputState& inputState() const = 0;

    virtual std::vector<std::string> listPrefabs() const = 0;

    virtual void update() = 0;

    virtual void onCanvasResize(uint32_t w, uint32_t h) = 0;

    virtual EventHandle listen(Event event, const EventHandler& handler) = 0;

    virtual ~EditorCore() = default;
};

using EditorCorePtr = std::unique_ptr<EditorCore>;

namespace lithic3d { class WindowDelegate; }

EditorCorePtr createEditorCore(const std::filesystem::path& projectRoot,
  lithic3d::WindowDelegate& windowDelegate);

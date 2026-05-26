#pragma once

#include "event_emitter.hpp"
#include <lithic3d/lithic3d.hpp>

struct EntityIdAndType
{
  lithic3d::EntityId id;
  std::string type;
};

class SceneEditMode
{
  public:
    enum class Event
    {
      AddOrRemoveEntity,
      EntitySelect,
      PrefabSelect
    };

    virtual void activate() = 0;
    virtual void deactivate() = 0;

    virtual std::vector<EntityIdAndType> getEntities() const = 0;
    virtual void setActivePrefab(const std::string& name) = 0;
    virtual const std::string& getActivePrefab() const = 0;
    virtual void instantiateActivePrefab() = 0;
    virtual void cancelActivePrefab() = 0;

    virtual void selectEntity(lithic3d::EntityId id, const std::string& type) = 0;
    virtual lithic3d::EntityId selectedEntity() const = 0;
    virtual void applyTransform() = 0;
    virtual void cancelTransform() = 0;

    virtual void update() = 0;

    virtual void onKeyDown(lithic3d::KeyboardKey key) = 0;
    virtual void onKeyUp(lithic3d::KeyboardKey key) = 0;
    virtual void onMouseLeftBtnDown() = 0;
    virtual void onMouseLeftBtnUp() = 0;
    virtual void onMouseMove(float x, float y) = 0;

    virtual EventHandle listen(Event event, const EventHandler& handler) = 0;

    virtual void saveChanges() = 0;

    virtual ~SceneEditMode() = default;
};

using SceneEditModePtr = std::unique_ptr<SceneEditMode>;

class EditorCore;

SceneEditModePtr createSceneEditMode(EditorCore& core);

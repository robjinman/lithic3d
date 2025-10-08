#pragma once

#include "ecs.hpp"
#include "sys_animation.hpp"
#include <memory>

struct Scene
{
  EntityId player = NULL_ENTITY;
  EntityId worldRoot = NULL_ENTITY;
  EntityId throwingModeIndicator = NULL_ENTITY;
  EntityId restartGamePrompt = NULL_ENTITY;
};

class SceneBuilder
{
  public:
    virtual Scene buildScene(uint32_t level) = 0;
    virtual EntityIdSet entities() const = 0;

    virtual ~SceneBuilder() = default;
};

using SceneBuilderPtr = std::unique_ptr<SceneBuilder>;

class EventSystem;
class GameOptionsManager;

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs,
  const GameOptionsManager& options);

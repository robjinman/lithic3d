#pragma once

#include "ecs.hpp"
#include <memory>

struct Scene
{
  EntityId player = NULL_ENTITY;
  EntityId worldRoot = NULL_ENTITY;
};

class SceneBuilder
{
  public:
    virtual Scene buildScene() = 0;
    virtual EntityIdSet entities() const = 0;

    virtual ~SceneBuilder() = default;
};

using SceneBuilderPtr = std::unique_ptr<SceneBuilder>;

class EventSystem;

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs);

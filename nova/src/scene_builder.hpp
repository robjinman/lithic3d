#pragma once

#include "ecs.hpp"
#include <memory>

class SceneBuilder
{
  public:
    virtual EntityId buildScene() = 0;
    virtual EntityIdSet entities() const = 0;

    virtual ~SceneBuilder() = default;
};

using SceneBuilderPtr = std::unique_ptr<SceneBuilder>;

class EventSystem;

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, Ecs& ecs);

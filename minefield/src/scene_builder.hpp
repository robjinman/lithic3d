#pragma once

#include <fge/ecs.hpp>
#include <fge/sys_animation_2d.hpp>
#include <memory>

struct Scene
{
  fge::EntityId player = fge::NULL_ENTITY;
  fge::EntityId worldRoot = fge::NULL_ENTITY;
  fge::EntityId throwingModeIndicator = fge::NULL_ENTITY;
  fge::EntityId restartGamePrompt = fge::NULL_ENTITY;
};

class SceneBuilder
{
  public:
    virtual Scene buildScene(uint32_t level) = 0;
    virtual fge::EntityIdSet entities() const = 0;

    virtual ~SceneBuilder() = default;
};

using SceneBuilderPtr = std::unique_ptr<SceneBuilder>;

namespace fge { class EventSystem; }
class GameOptionsManager;

SceneBuilderPtr createSceneBuilder(fge::EventSystem& eventSystem, fge::Ecs& ecs,
  const GameOptionsManager& options);

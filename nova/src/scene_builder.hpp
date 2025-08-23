#pragma once

#include "component_store.hpp"
#include <memory>

class SceneBuilder
{
  public:
    virtual EntityId buildScene() = 0;

    virtual ~SceneBuilder() = default;
};

using SceneBuilderPtr = std::unique_ptr<SceneBuilder>;

class EventSystem;
class ComponentStore;
class SysAnimation;
class SysBehaviour;
class SysGrid;
class SysRender;

SceneBuilderPtr createSceneBuilder(EventSystem& eventSystem, ComponentStore& componentStore,
  SysAnimation& sysAnimation, SysBehaviour& sysBehaviour, SysGrid& sysGrid,
  SysRender& sysRender);

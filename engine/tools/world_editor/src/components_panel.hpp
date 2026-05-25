#pragma once

#include <lithic3d/entity_id.hpp>
#include <memory>

class wxWindow;

class ComponentsPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;

    virtual void onEntitySelect(lithic3d::EntityId entityId) = 0;
    virtual void onPrefabSelect(const std::string& prefab) = 0;

    virtual ~ComponentsPanel() = default;
};

using ComponentsPanelPtr = std::unique_ptr<ComponentsPanel>;

class EditorCore;

ComponentsPanelPtr createComponentsPanel(wxWindow* parent, EditorCore& editorCore);

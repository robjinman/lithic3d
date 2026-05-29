#pragma once

#include <lithic3d/entity_id.hpp>
#include <lithic3d/systems.hpp>
#include <memory>

class wxWindow;

class ComponentsPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;

    virtual void onEntitySelect(lithic3d::EntityId entityId) = 0;
    virtual void repopulate() = 0;
    virtual lithic3d::SystemId getSelectedSystem() const = 0;
    virtual lithic3d::ComponentDataPtr getComponentData() const = 0;

    virtual ~ComponentsPanel() = default;
};

using ComponentsPanelPtr = std::unique_ptr<ComponentsPanel>;

class EditorCore;

ComponentsPanelPtr createComponentsPanel(wxWindow* parent, EditorCore& editorCore);

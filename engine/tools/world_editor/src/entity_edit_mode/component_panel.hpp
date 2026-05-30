#pragma once

#include <lithic3d/systems.hpp>
#include <lithic3d/entity_id.hpp>
#include <wx/wx.h>
#include <memory>

class wxWindow;

class ComponentPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;
    virtual void populate(lithic3d::EntityId entityId) = 0;
    virtual lithic3d::SystemId systemId() const = 0;
    //virtual void repopulate() = 0;
    virtual bool hasChanges() const = 0;
    //virtual lithic3d::ComponentDataPtr getComponentData() const = 0;

    virtual ~ComponentPanel() = default;
};

using ComponentPanelPtr = std::unique_ptr<ComponentPanel>;

wxDECLARE_EVENT(EComponentChange, wxCommandEvent);

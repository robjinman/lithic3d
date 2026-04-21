#pragma once

#include <wx/wx.h>
#include <memory>

class PrefabsPanel
{
  public:
    virtual void populate() = 0;
    virtual wxPanel* getWxPtr() = 0;

    virtual ~PrefabsPanel() = default;
};

using PrefabsPanelPtr = std::unique_ptr<PrefabsPanel>;

class WorldEditor;

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, WorldEditor& worldEditor);

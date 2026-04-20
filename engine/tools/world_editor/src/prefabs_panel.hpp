#pragma once

#include <wx/wx.h>
#include <memory>
#include <filesystem>

class PrefabsPanel
{
  public:
    virtual void populate() = 0;
    virtual wxPanel* getWxPtr() = 0;

    virtual ~PrefabsPanel() = default;
};

using PrefabsPanelPtr = std::unique_ptr<PrefabsPanel>;

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, const std::filesystem::path& projectRoot);

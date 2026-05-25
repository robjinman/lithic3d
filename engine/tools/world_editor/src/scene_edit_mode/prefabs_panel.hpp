#pragma once

#include <memory>

class wxPanel;
class wxWindow;

class PrefabsPanel
{
  public:
    virtual void populate() = 0;
    virtual wxPanel* getWxPtr() = 0;

    virtual ~PrefabsPanel() = default;
};

using PrefabsPanelPtr = std::unique_ptr<PrefabsPanel>;

class EditorCore;
class SceneEditMode;

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, EditorCore& editorCore, SceneEditMode& mode);

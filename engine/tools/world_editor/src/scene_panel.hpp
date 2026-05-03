#pragma once

#include <memory>
#include <vector>

class wxPanel;
class wxWindow;

namespace lithic3d { struct Entity; }

class ScenePanel
{
  public:
    virtual void populate(const std::vector<lithic3d::Entity>& entities) = 0;
    virtual wxPanel* getWxPtr() = 0;

    virtual ~ScenePanel() = default;
};

using ScenePanelPtr = std::unique_ptr<ScenePanel>;

class WorldEditor;

ScenePanelPtr createScenePanel(wxWindow* parent, WorldEditor& worldEditor);

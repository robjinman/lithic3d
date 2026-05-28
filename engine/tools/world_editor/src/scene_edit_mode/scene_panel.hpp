#pragma once

#include <memory>
#include <vector>

class wxPanel;
class wxWindow;
struct EntityIdAndType;

class ScenePanel
{
  public:
    virtual void populate(const std::vector<EntityIdAndType>& entities) = 0;
    virtual wxPanel* getWxPtr() = 0;

    virtual ~ScenePanel() = default;
};

using ScenePanelPtr = std::unique_ptr<ScenePanel>;

class SceneEditMode;
class EditorCore;

ScenePanelPtr createScenePanel(wxWindow* parent, EditorCore& core, SceneEditMode& mode);

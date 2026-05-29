#include "scene_edit_mode/scene_edit_mode.hpp"
#include "mode_ui.hpp"
#include "tools.hpp"
#include "editor_core.hpp"
#include "prefabs_panel.hpp"
#include "cursor_panel.hpp"
#include "scene_panel.hpp"
#include <wx/wx.h>
#include <wx/notebook.h>

using namespace lithic3d;

namespace
{

class SceneEditModeUi : public ModeUi
{
  public:
    SceneEditModeUi(const Panels& panels, EditorCore& editorCore);

    void activate() override;
    void deactivate() override;

    void update() override;

    void onKeyDown(KeyboardKey key) override;
    void onKeyUp(KeyboardKey key) override;
    void onMouseLeftBtnDown() override;
    void onMouseLeftBtnUp() override;
    void onMouseMove(float x, float y) override;

    void saveChanges() override;

  private:
    Panels m_panels;
    SceneEditModePtr m_mode;
    EditorCore& m_core;
    PrefabsPanelPtr m_prefabsPanel = nullptr;
    ScenePanelPtr m_scenePanel = nullptr;
    CursorPanelPtr m_cursorPanel = nullptr;

    void onApplyTransform();
    void onToolToggleOn(Tool tool);
    void onToolToggleOff(Tool tool);
};

SceneEditModeUi::SceneEditModeUi(const Panels& panels, EditorCore& editorCore)
  : m_panels(panels)
  , m_core(editorCore)
{
  m_mode = createSceneEditMode(m_core);

  m_prefabsPanel = createPrefabsPanel(m_panels.panel1, m_core, *m_mode);
  m_scenePanel = createScenePanel(m_panels.panel1, m_core, *m_mode);
  m_cursorPanel = createCursorPanel(m_panels.sidebar, m_core);
  m_cursorPanel->getWxPtr()->Hide();

  m_cursorPanel->getWxPtr()->Bind(ECancelActiveTransform,
    [this](wxEvent&) { m_mode->cancelTransform(); });

  m_cursorPanel->getWxPtr()->Bind(EApplyActiveTransform,
    [this](wxEvent&) { onApplyTransform(); });

  m_scenePanel->getWxPtr()->Bind(EToolToggleOn,
    [this](wxCommandEvent& e) { onToolToggleOn(static_cast<Tool>(e.GetInt())); });

  m_scenePanel->getWxPtr()->Bind(EToolToggleOff,
    [this](wxCommandEvent& e) { onToolToggleOff(static_cast<Tool>(e.GetInt())); });

  m_prefabsPanel->populate();
  m_scenePanel->populate(m_mode->getEntities());
}

void SceneEditModeUi::onApplyTransform()
{
  m_mode->applyTransform();
  m_scenePanel->onApplyTransform();
}

void SceneEditModeUi::onToolToggleOn(Tool tool)
{
  switch (tool) {
    case Tool::BoundingBox:
      std::cout << "Scene edit mode, tool bounding box toggled ON\n";
      break;
    default: break;
  }
}

void SceneEditModeUi::onToolToggleOff(Tool tool)
{
  switch (tool) {
    case Tool::BoundingBox:
      std::cout << "Scene edit mode, tool bounding box toggled OFF\n";
      break;
    default: break;
  }
}

void SceneEditModeUi::onKeyDown(KeyboardKey key)
{
  m_mode->onKeyDown(key);
}

void SceneEditModeUi::onKeyUp(KeyboardKey key)
{
  m_mode->onKeyUp(key);
}

void SceneEditModeUi::onMouseLeftBtnDown()
{
  m_mode->onMouseLeftBtnDown();
}

void SceneEditModeUi::onMouseLeftBtnUp()
{
  m_mode->onMouseLeftBtnUp();
}

void SceneEditModeUi::onMouseMove(float x, float y)
{
  m_mode->onMouseMove(x, y);
}

void SceneEditModeUi::update()
{
  m_mode->update();
}

void SceneEditModeUi::saveChanges()
{
  m_mode->saveChanges();
}

void SceneEditModeUi::activate()
{
  m_panels.panel1->AddPage(m_prefabsPanel->getWxPtr(), "Prefabs");
  m_panels.panel1->AddPage(m_scenePanel->getWxPtr(), "Scene");

  m_panels.sidebar->GetSizer()->Add(m_cursorPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_cursorPanel->getWxPtr()->Show();
  m_panels.sidebar->Layout();

  m_mode->activate();
}

void SceneEditModeUi::deactivate()
{
  m_mode->deactivate();

  while (m_panels.panel1->GetPageCount() > 0) {
    m_panels.panel1->RemovePage(0);
  }

  m_cursorPanel->getWxPtr()->Hide();
  m_panels.sidebar->GetSizer()->Remove(0);
}

} // namespace

ModeUiPtr createSceneEditModeUi(const Panels& panels, EditorCore& editorCore)
{
  return std::make_unique<SceneEditModeUi>(panels, editorCore);
}

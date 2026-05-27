#include "scene_edit_mode/scene_edit_mode.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "prefabs_panel.hpp"
#include "scene_panel.hpp"
#include "current_transform_panel.hpp"
#include "components_panel.hpp"
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
    CurrentTransformPanelPtr m_currentTransformPanel = nullptr;
    ComponentsPanelPtr m_componentsPanel = nullptr;
    EventHandle m_onEntitySelect;

    void onEntitySelect();
};

SceneEditModeUi::SceneEditModeUi(const Panels& panels, EditorCore& editorCore)
  : m_panels(panels)
  , m_core(editorCore)
{
  m_mode = createSceneEditMode(m_core);

  m_prefabsPanel = createPrefabsPanel(m_panels.panel1, m_core, *m_mode);
  m_scenePanel = createScenePanel(m_panels.panel1, *m_mode);
  m_currentTransformPanel = createCurrentTransformPanel(m_panels.sidebar, m_core);
  m_currentTransformPanel->getWxPtr()->Hide();
  m_componentsPanel = createComponentsPanel(m_panels.panel2, m_core);

  m_prefabsPanel->populate();
  m_scenePanel->populate(m_mode->getEntities());

  m_onEntitySelect = m_mode->listen(SceneEditMode::Event::EntitySelect,
    [this]() { onEntitySelect(); });
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

void SceneEditModeUi::onEntitySelect()
{
  m_componentsPanel->onEntitySelect(m_mode->selectedEntity());
}

void SceneEditModeUi::activate()
{
  m_panels.panel1->AddPage(m_prefabsPanel->getWxPtr(), "Prefabs");
  m_panels.panel1->AddPage(m_scenePanel->getWxPtr(), "Scene");
  m_panels.panel2->AddPage(m_componentsPanel->getWxPtr(), "Components");

  m_panels.sidebar->GetSizer()->Add(m_currentTransformPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_currentTransformPanel->getWxPtr()->Show();
  m_panels.sidebar->Layout();

  m_mode->activate();
}

void SceneEditModeUi::deactivate()
{
  m_mode->deactivate();

  while (m_panels.panel1->GetPageCount() > 0) {
    m_panels.panel1->RemovePage(0);
  }

  while (m_panels.panel2->GetPageCount() > 0) {
    m_panels.panel2->RemovePage(0);
  }

  m_currentTransformPanel->getWxPtr()->Hide();
  m_panels.sidebar->GetSizer()->Remove(0);
}

} // namespace

ModeUiPtr createSceneEditModeUi(const Panels& panels, EditorCore& editorCore)
{
  return std::make_unique<SceneEditModeUi>(panels, editorCore);
}

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
    SceneEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel, EditorCore& editorCore);

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
    wxNotebook& m_topPanel;
    wxNotebook& m_bottomPanel;
    SceneEditModePtr m_mode;
    EditorCore& m_core;
    PrefabsPanelPtr m_prefabsPanel = nullptr;
    ScenePanelPtr m_scenePanel = nullptr;
    CurrentTransformPanelPtr m_currentTransformPanel = nullptr;
    ComponentsPanelPtr m_componentsPanel = nullptr;

    void onEntitySelect();
};

SceneEditModeUi::SceneEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore)
  : m_topPanel(topPanel)
  , m_bottomPanel(bottomPanel)
  , m_core(editorCore)
{
  m_mode = createSceneEditMode(m_core);

  m_prefabsPanel = createPrefabsPanel(&m_topPanel, m_core, *m_mode);
  m_scenePanel = createScenePanel(&m_topPanel, *m_mode);
  m_currentTransformPanel = createCurrentTransformPanel(&m_bottomPanel, m_core);
  m_componentsPanel = createComponentsPanel(&m_bottomPanel, m_core);

  m_prefabsPanel->populate();
  m_scenePanel->populate(m_mode->getEntities());

  m_mode->listen(SceneEditMode::Event::EntitySelect, [this]() { onEntitySelect(); });
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
  m_topPanel.AddPage(m_prefabsPanel->getWxPtr(), "Prefabs");
  m_topPanel.AddPage(m_scenePanel->getWxPtr(), "Scene");
  m_bottomPanel.AddPage(m_currentTransformPanel->getWxPtr(), "Transform");
  m_bottomPanel.AddPage(m_componentsPanel->getWxPtr(), "Components");
}

void SceneEditModeUi::deactivate()
{
  while (m_topPanel.GetPageCount() > 0) {
    m_topPanel.RemovePage(0);
  }

  while (m_bottomPanel.GetPageCount() > 0) {
    m_bottomPanel.RemovePage(0);
  }
}

} // namespace

ModeUiPtr createSceneEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore)
{
  return std::make_unique<SceneEditModeUi>(topPanel, bottomPanel, editorCore);
}

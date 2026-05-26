#include "prefab_edit_mode/prefab_edit_mode.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "components_panel.hpp"
#include "current_transform_panel.hpp"
#include <wx/wx.h>

using namespace lithic3d;

namespace
{

class PrefabEditModeUi : public ModeUi
{
  public:
    PrefabEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel, EditorCore& core);

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
    EditorCore& m_core;
    PrefabEditModePtr m_mode;
    ComponentsPanelPtr m_componentsPanel;
    CurrentTransformPanelPtr m_currentTransformPanel;
    wxListBox* m_prefabsListBox;

    void onPrefabSelection();
};

PrefabEditModeUi::PrefabEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel, EditorCore& core)
  : m_topPanel(topPanel)
  , m_bottomPanel(bottomPanel)
  , m_core(core)
{
  m_mode = createPrefabEditMode(m_core);

  m_currentTransformPanel = createCurrentTransformPanel(&m_bottomPanel, m_core);
  m_componentsPanel = createComponentsPanel(&m_bottomPanel, m_core);

  m_prefabsListBox = new wxListBox{&m_topPanel, wxID_ANY};
  m_prefabsListBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
}

void PrefabEditModeUi::activate()
{
  m_prefabsListBox->Clear();
  auto prefabs = m_core.listPrefabs();
  for (size_t i = 0; i < prefabs.size(); ++i) {
    m_prefabsListBox->Insert(prefabs[i], i);
  }

  m_topPanel.AddPage(m_prefabsListBox, "Prefabs");
  m_bottomPanel.AddPage(m_currentTransformPanel->getWxPtr(), "Transform");
  m_bottomPanel.AddPage(m_componentsPanel->getWxPtr(), "Components");

  m_mode->activate();
}

void PrefabEditModeUi::deactivate()
{
  m_mode->deactivate();

  while (m_topPanel.GetPageCount() > 0) {
    m_topPanel.RemovePage(0);
  }

  while (m_bottomPanel.GetPageCount() > 0) {
    m_bottomPanel.RemovePage(0);
  }
}

void PrefabEditModeUi::onPrefabSelection()
{
  m_mode->setActivePrefab(m_prefabsListBox->GetStringSelection().ToStdString());
}

void PrefabEditModeUi::update()
{
  m_mode->update();
}

void PrefabEditModeUi::onKeyDown(KeyboardKey key)
{
  m_mode->onKeyDown(key);
}

void PrefabEditModeUi::onKeyUp(KeyboardKey key)
{
  m_mode->onKeyUp(key);
}

void PrefabEditModeUi::onMouseLeftBtnDown()
{
  m_mode->onMouseLeftBtnDown();
}

void PrefabEditModeUi::onMouseLeftBtnUp()
{
  m_mode->onMouseLeftBtnUp();
}

void PrefabEditModeUi::onMouseMove(float x, float y)
{
  m_mode->onMouseMove(x, y);
}

void PrefabEditModeUi::saveChanges()
{
  m_mode->saveChanges();
}

} // namespace

ModeUiPtr createPrefabEditModeUi(wxNotebook& topPanel, wxNotebook& bottomPanel,
  EditorCore& editorCore)
{
  return std::make_unique<PrefabEditModeUi>(topPanel, bottomPanel, editorCore);
}

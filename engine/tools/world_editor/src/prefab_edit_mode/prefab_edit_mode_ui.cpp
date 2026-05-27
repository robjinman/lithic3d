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
    PrefabEditModeUi(const Panels& panels, EditorCore& core);

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
    EditorCore& m_core;
    PrefabEditModePtr m_mode;
    ComponentsPanelPtr m_componentsPanel;
    CurrentTransformPanelPtr m_currentTransformPanel;
    wxListBox* m_prefabsListBox;

    void onPrefabSelection();
};

PrefabEditModeUi::PrefabEditModeUi(const Panels& panels, EditorCore& core)
  : m_panels(panels)
  , m_core(core)
{
  m_mode = createPrefabEditMode(m_core);

  m_currentTransformPanel = createCurrentTransformPanel(m_panels.sidebar, m_core);
  m_currentTransformPanel->getWxPtr()->Hide();

  m_componentsPanel = createComponentsPanel(m_panels.panel2, m_core);

  m_prefabsListBox = new wxListBox{m_panels.panel1, wxID_ANY};
  m_prefabsListBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
}

void PrefabEditModeUi::activate()
{
  m_prefabsListBox->Clear();
  auto prefabs = m_core.listPrefabs();
  for (size_t i = 0; i < prefabs.size(); ++i) {
    m_prefabsListBox->Insert(prefabs[i], i);
  }

  m_panels.panel1->AddPage(m_prefabsListBox, "Prefabs");
  m_panels.panel2->AddPage(m_componentsPanel->getWxPtr(), "Components");
  m_panels.sidebar->GetSizer()->Add(m_currentTransformPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_currentTransformPanel->getWxPtr()->Show();
  m_panels.sidebar->Layout();

  m_mode->activate();
}

void PrefabEditModeUi::deactivate()
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

void PrefabEditModeUi::onPrefabSelection()
{
  m_mode->setActivePrefab(m_prefabsListBox->GetStringSelection().ToStdString());
  auto entityId = m_mode->instantiatedPrefabId();
  assert(entityId != NULL_ENTITY_ID);
  m_componentsPanel->onEntitySelect(entityId);
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

ModeUiPtr createPrefabEditModeUi(const Panels& panels, EditorCore& editorCore)
{
  return std::make_unique<PrefabEditModeUi>(panels, editorCore);
}

#include "prefab_edit_mode/prefab_edit_mode.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "tools.hpp"
#include "components_panel.hpp"
#include "cursor_panel.hpp"
#include <lithic3d/lithic3d.hpp>
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
    CursorPanelPtr m_cursorPanel;
    wxPanel* m_panel1Content = nullptr;
    wxListBox* m_prefabsListBox;

    void constructPanel1Content();
    void onPrefabSelection();
    void onToolToggleOn(Tool tool);
    void onToolToggleOff(Tool tool);
    void toggleBoundingBoxToolOn();
};

PrefabEditModeUi::PrefabEditModeUi(const Panels& panels, EditorCore& core)
  : m_panels(panels)
  , m_core(core)
{
  m_mode = createPrefabEditMode(m_core);

  m_cursorPanel = createCursorPanel(m_panels.sidebar, m_core);
  m_cursorPanel->getWxPtr()->Hide();

  constructPanel1Content();
}

void PrefabEditModeUi::constructPanel1Content()
{
  m_panel1Content = new wxPanel(m_panels.panel1, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_prefabsListBox = new wxListBox{m_panel1Content, wxID_ANY};

  auto staticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel1Content, "Components");
  m_componentsPanel = createComponentsPanel(staticBoxSizer->GetStaticBox(), m_core);
  staticBoxSizer->Add(m_componentsPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(m_prefabsListBox, wxSizerFlags(1).Expand());
  vbox->Add(staticBoxSizer, wxSizerFlags(1).Expand());

  m_panel1Content->SetSizer(vbox);

  m_componentsPanel->getWxPtr()->Bind(EToolToggleOn,
    [this](wxCommandEvent& e) { onToolToggleOn(static_cast<Tool>(e.GetInt())); });

  m_componentsPanel->getWxPtr()->Bind(EToolToggleOff,
    [this](wxCommandEvent& e) { onToolToggleOff(static_cast<Tool>(e.GetInt())); });

  m_prefabsListBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
}

void PrefabEditModeUi::onToolToggleOn(Tool tool)
{
  switch (tool) {
    case Tool::BoundingBox:
      toggleBoundingBoxToolOn();
      break;
  }
}

void PrefabEditModeUi::toggleBoundingBoxToolOn()
{
  assert(m_componentsPanel->getSelectedSystem() == Systems::Collision);

  auto data = m_componentsPanel->getComponentData();

  if (data->typeId() == typeid(DStaticBox).hash_code()) {
    auto& box = dynamic_cast<const ComponentDataWrapper<DStaticBox>&>(*data);
    m_mode->setActiveBoundingBox(box.data().boundingBox);
  }
  else if (data->typeId() == typeid(DDynamicBox).hash_code()) {
    auto& box = dynamic_cast<const ComponentDataWrapper<DDynamicBox>&>(*data);
    m_mode->setActiveBoundingBox(box.data().boundingBox);
  }
  else {
    EXCEPTION("Unexpected collision component type");
  }
}

void PrefabEditModeUi::onToolToggleOff(Tool tool)
{
  switch (tool) {
    case Tool::BoundingBox:
      m_mode->cancelActiveBoundingBox();
      break;
  }
}

void PrefabEditModeUi::activate()
{
  m_prefabsListBox->Clear();
  auto prefabs = m_core.listPrefabs();
  for (size_t i = 0; i < prefabs.size(); ++i) {
    m_prefabsListBox->Insert(prefabs[i], i);
  }

  m_panels.panel1->AddPage(m_panel1Content, "Prefabs");
  m_panels.sidebar->GetSizer()->Add(m_cursorPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_cursorPanel->getWxPtr()->Show();
  m_panels.sidebar->Layout();

  m_mode->activate();
}

void PrefabEditModeUi::deactivate()
{
  m_mode->deactivate();

  while (m_panels.panel1->GetPageCount() > 0) {
    m_panels.panel1->RemovePage(0);
  }

  m_cursorPanel->getWxPtr()->Hide();
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

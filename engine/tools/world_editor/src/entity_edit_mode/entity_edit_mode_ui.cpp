#include "entity_edit_mode/entity_edit_mode.hpp"
#include "entity_edit_mode/components_panel.hpp"
#include "entity_edit_mode/component_panel.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "tools.hpp"
#include "cursor_panel.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>

using namespace lithic3d;

namespace
{

class EntityEditModeUi : public ModeUi
{
  public:
    EntityEditModeUi(const Panels& panels, EditorCore& core);

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
    EntityEditModePtr m_mode;

    ComponentsPanelPtr m_componentsPanel;
    CursorPanelPtr m_cursorPanel;
    wxPanel* m_rightSidebarContent = nullptr;
    wxListBox* m_lstPrefabs = nullptr;
    wxListBox* m_lstEntities = nullptr;

    void populatePrefabs();
    void populateEntities();
    void onInstanceSelection();
    void onPrefabSelection();
    void onApplyActiveTransform();
};

EntityEditModeUi::EntityEditModeUi(const Panels& panels, EditorCore& core)
  : m_panels(panels)
  , m_core(core)
{
  m_mode = createEntityEditMode(m_core);

  m_cursorPanel = createCursorPanel(m_panels.leftSidebar, m_core);
  m_cursorPanel->getWxPtr()->Hide();

  m_cursorPanel->getWxPtr()->Bind(ECancelActiveTransform,
    [this](wxEvent&) { m_mode->cancelTransform(); });

  m_cursorPanel->getWxPtr()->Bind(EApplyActiveTransform,
    [this](wxEvent&) { onApplyActiveTransform(); });

  m_rightSidebarContent = new wxPanel(m_panels.rightSidebar, wxID_ANY);
  m_rightSidebarContent->Hide();
  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto notebook = new wxNotebook(m_rightSidebarContent, wxID_ANY);

  m_lstPrefabs = new wxListBox(notebook, wxID_ANY);
  m_lstEntities = new wxListBox(notebook, wxID_ANY);

  notebook->AddPage(m_lstPrefabs, "Prefabs");
  notebook->AddPage(m_lstEntities, "Scene");

  vbox->Add(notebook, wxSizerFlags(1).Expand());

  auto staticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_rightSidebarContent, "Components");
  m_componentsPanel = createComponentsPanel(staticBoxSizer->GetStaticBox(), m_core, *m_mode);
  staticBoxSizer->Add(m_componentsPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(staticBoxSizer, wxSizerFlags(1).Expand());

  m_rightSidebarContent->SetSizer(vbox);

  m_lstPrefabs->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
  m_lstEntities->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onInstanceSelection(); });

  populatePrefabs();
  //populateEntities();
}

void EntityEditModeUi::onInstanceSelection()
{
  // TODO
}

void EntityEditModeUi::populatePrefabs()
{
  auto prefabNames = m_core.listPrefabs();

  for (size_t i = 0; i < prefabNames.size(); ++i) {
     m_lstPrefabs->Insert(prefabNames[i], i);
  }
}

void EntityEditModeUi::populateEntities()
{
  m_lstEntities->Clear();

  //auto entities = m_sceneEditMode.getEntities();

  //for (size_t i = 0; i < entities.size(); ++i) {
  //  m_lstEntities->Insert(STR("[" << entities[i].id << "] " << entities[i].type), i,
  //    new EntityIdAndType{entities[i]});
  //}
}

void EntityEditModeUi::activate()
{
  m_panels.rightSidebar->GetSizer()->Add(m_rightSidebarContent, wxSizerFlags(1).Expand());
  m_rightSidebarContent->Show();    // TODO: Is hide/show necessary?
  m_panels.rightSidebar->Layout();

  m_panels.leftSidebar->GetSizer()->Add(m_cursorPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_cursorPanel->getWxPtr()->Show();
  m_panels.leftSidebar->Layout();

  m_mode->activate();
}

void EntityEditModeUi::deactivate()
{
  m_mode->deactivate();

  m_rightSidebarContent->Hide();
  m_panels.rightSidebar->GetSizer()->Remove(0);

  m_cursorPanel->getWxPtr()->Hide();
  m_panels.leftSidebar->GetSizer()->Remove(0);
}

void EntityEditModeUi::onApplyActiveTransform()
{
  m_mode->applyTransform();
  m_componentsPanel->repopulateFromMode();
}

void EntityEditModeUi::onPrefabSelection()
{
  m_mode->setActivePrefab(m_lstPrefabs->GetStringSelection().ToStdString());
  auto entityId = m_mode->instantiatedPrefabId();
  assert(entityId != NULL_ENTITY_ID);
  m_componentsPanel->onEntitySelect(entityId);
}

void EntityEditModeUi::update()
{
  m_mode->update();
}

void EntityEditModeUi::onKeyDown(KeyboardKey key)
{
  m_mode->onKeyDown(key);
}

void EntityEditModeUi::onKeyUp(KeyboardKey key)
{
  m_mode->onKeyUp(key);
}

void EntityEditModeUi::onMouseLeftBtnDown()
{
  m_mode->onMouseLeftBtnDown();
}

void EntityEditModeUi::onMouseLeftBtnUp()
{
  m_mode->onMouseLeftBtnUp();
}

void EntityEditModeUi::onMouseMove(float x, float y)
{
  m_mode->onMouseMove(x, y);
}

void EntityEditModeUi::saveChanges()
{
  m_mode->saveChanges();
}

} // namespace

ModeUiPtr createEntityEditModeUi(const Panels& panels, EditorCore& editorCore)
{
  return std::make_unique<EntityEditModeUi>(panels, editorCore);
}

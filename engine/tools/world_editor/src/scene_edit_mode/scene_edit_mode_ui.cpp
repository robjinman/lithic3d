#include "scene_edit_mode/scene_edit_mode.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "cursor_panel.hpp"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>

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
    wxNotebook* m_notebook = nullptr;
    wxListBox* m_lstPrefabs = nullptr;
    wxListView* m_lstEntities = nullptr;
    CursorPanelPtr m_cursorPanel = nullptr;
    EventHandle m_onAddOrRemoveEntity;

    void populatePrefabs();
    void populateEntities();
    void onInstanceSelection();
    void onPrefabSelection();
    void onInstanceShow(wxEvent& e);
    void onInstanceHide(wxEvent& e);
};

SceneEditModeUi::SceneEditModeUi(const Panels& panels, EditorCore& editorCore)
  : m_panels(panels)
  , m_core(editorCore)
{
  m_mode = createSceneEditMode(m_core);

  m_cursorPanel = createCursorPanel(m_panels.leftSidebar, m_core);
  m_cursorPanel->getWxPtr()->Hide();

  m_cursorPanel->getWxPtr()->Bind(ECancelActiveTransform,
    [this](wxEvent&) { m_mode->cancelTransform(); });

  m_cursorPanel->getWxPtr()->Bind(EApplyActiveTransform,
    [this](wxEvent&) { m_mode->applyTransform(); });

  m_notebook = new wxNotebook(m_panels.rightSidebar, wxID_ANY);

  m_lstPrefabs = new wxListBox(m_notebook, wxID_ANY);
  m_lstEntities = new wxListView(m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    wxLC_REPORT | wxLC_SINGLE_SEL);
  m_lstEntities->EnableCheckBoxes();

  m_notebook->AddPage(m_lstPrefabs, "Prefabs");
  m_notebook->AddPage(m_lstEntities, "Scene");

  m_lstPrefabs->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
  m_lstEntities->Bind(wxEVT_COMMAND_LIST_ITEM_SELECTED,
    [this](wxEvent&) { onInstanceSelection(); });
  m_lstEntities->Bind(wxEVT_LIST_ITEM_CHECKED, [this](wxEvent& e) { onInstanceShow(e); });
  m_lstEntities->Bind(wxEVT_LIST_ITEM_UNCHECKED, [this](wxEvent& e) { onInstanceHide(e); });

  m_onAddOrRemoveEntity = m_mode->listen(SceneEditMode::Event::AddOrRemoveEntity,
    [this]() { populateEntities(); });

  populatePrefabs();
  populateEntities();
}

void SceneEditModeUi::onInstanceSelection()
{
  auto index = m_lstEntities->GetFirstSelected();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_lstEntities->GetItemData(index));
  m_mode->selectEntity(entity.id);
}

void SceneEditModeUi::onInstanceShow(wxEvent& e)
{
  auto event = dynamic_cast<wxListEvent&>(e);
  auto index = event.GetIndex();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_lstEntities->GetItemData(index));
  m_mode->showEntity(entity.id);
}

void SceneEditModeUi::onInstanceHide(wxEvent& e)
{
  auto event = dynamic_cast<wxListEvent&>(e);
  auto index = event.GetIndex();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_lstEntities->GetItemData(index));
  m_mode->hideEntity(entity.id);
}

void SceneEditModeUi::onPrefabSelection()
{
  auto selected = m_lstPrefabs->GetStringSelection().ToStdString();

  if (!selected.empty()) {
    m_mode->setActivePrefab(selected);
  }
}

void SceneEditModeUi::populatePrefabs()
{
  auto prefabNames = m_core.listPrefabs();

  for (size_t i = 0; i < prefabNames.size(); ++i) {
     m_lstPrefabs->Insert(prefabNames[i], i);
  }
}

void SceneEditModeUi::populateEntities()
{
  m_lstEntities->ClearAll();
  m_lstEntities->AppendColumn("Entity");

  auto entities = m_mode->getEntities();

  for (size_t i = 0; i < entities.size(); ++i) {
    m_lstEntities->InsertItem(i, STR("[" << entities[i].id << "] " << entities[i].type));
    m_lstEntities->SetItemData(i, reinterpret_cast<long>(new EntityIdAndType{entities[i]}));
    m_lstEntities->CheckItem(i, true);
  }

  // Doesn't work on OS X
  //m_lstEntities->SetColumnWidth(0, m_lstEntities->GetClientSize().GetWidth())

  m_lstEntities->SetColumnWidth(0, 200); // TODO: Magic number
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
  m_panels.rightSidebar->GetSizer()->Add(m_notebook, wxSizerFlags(1).Expand());
  m_notebook->Show();
  m_panels.rightSidebar->Layout();

  m_panels.leftSidebar->GetSizer()->Add(m_cursorPanel->getWxPtr(),
    wxSizerFlags(1).Expand().Border(wxALL, 10));
  m_cursorPanel->getWxPtr()->Show();
  m_panels.leftSidebar->Layout();

  m_mode->activate();
}

void SceneEditModeUi::deactivate()
{
  m_mode->deactivate();

  m_notebook->Hide();
  m_panels.rightSidebar->GetSizer()->Remove(0);

  m_cursorPanel->getWxPtr()->Hide();
  m_panels.leftSidebar->GetSizer()->Remove(0);
}

} // namespace

ModeUiPtr createSceneEditModeUi(const Panels& panels, EditorCore& editorCore)
{
  return std::make_unique<SceneEditModeUi>(panels, editorCore);
}

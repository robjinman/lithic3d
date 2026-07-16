#include "scene_edit_mode/scene_edit_mode.hpp"
#include "mode_ui.hpp"
#include "editor_core.hpp"
#include "cursor_panel.hpp"
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
    wxNotebook* m_notebook = nullptr;
    wxListBox* m_lstPrefabs = nullptr;
    wxListBox* m_lstEntities = nullptr;
    CursorPanelPtr m_cursorPanel = nullptr;
    EventHandle m_onAddOrRemoveEntity;

    void populatePrefabs();
    void populateEntities();
    void onInstanceSelection();
    void onPrefabSelection();
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
  m_lstEntities = new wxListBox(m_notebook, wxID_ANY);

  m_notebook->AddPage(m_lstPrefabs, "Prefabs");
  m_notebook->AddPage(m_lstEntities, "Scene");

  m_lstPrefabs->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onPrefabSelection(); });
  m_lstEntities->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent&) { onInstanceSelection(); });

  m_onAddOrRemoveEntity = m_mode->listen(SceneEditMode::Event::AddOrRemoveEntity,
    [this]() { populateEntities(); });

  populatePrefabs();
  populateEntities();
}

void SceneEditModeUi::onInstanceSelection()
{
  auto index = m_lstEntities->GetSelection();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_lstEntities->GetClientData(index));
  m_mode->selectEntity(entity.id);
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
  m_lstEntities->Clear();

  auto entities = m_mode->getEntities();

  for (size_t i = 0; i < entities.size(); ++i) {
    m_lstEntities->Insert(STR("[" << entities[i].id << "] " << entities[i].type), i,
      new EntityIdAndType{entities[i]});
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

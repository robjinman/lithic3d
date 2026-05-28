#include "scene_edit_mode/scene_panel.hpp"
#include "scene_edit_mode/scene_edit_mode.hpp"
#include "components_panel.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

class ScenePanelImpl : public ScenePanel
{
  public:
    ScenePanelImpl(wxWindow* parent, EditorCore& core, SceneEditMode& mode);

    void populate(const std::vector<EntityIdAndType>& entities) override;
    wxPanel* getWxPtr() override;

  private:
    void onInstanceSelection(wxEvent& e);

    SceneEditMode& m_mode;
    wxPanel* m_basePanel = nullptr;
    wxListBox* m_listBox = nullptr;
    ComponentsPanelPtr m_componentsPanel = nullptr;
    EventHandle m_onAddOrRemoveEntity;
    EventHandle m_onEntitySelect;

    void onAddOrRemoveEntity();
};

ScenePanelImpl::ScenePanelImpl(wxWindow* parent, EditorCore& core, SceneEditMode& mode)
  : m_mode(mode)
{
  m_basePanel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_listBox = new wxListBox{m_basePanel, wxID_ANY};

  auto staticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_basePanel, "Components");
  m_componentsPanel = createComponentsPanel(staticBoxSizer->GetStaticBox(), core);
  staticBoxSizer->Add(m_componentsPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(m_listBox, wxSizerFlags(1).Expand());
  vbox->Add(staticBoxSizer, wxSizerFlags(1).Expand());

  m_basePanel->SetSizer(vbox);
  m_basePanel->Layout();

  m_listBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent& e) { onInstanceSelection(e); });

  m_onAddOrRemoveEntity = m_mode.listen(SceneEditMode::Event::AddOrRemoveEntity,
    [this]() { onAddOrRemoveEntity(); });
}

void ScenePanelImpl::onAddOrRemoveEntity()
{
  populate(m_mode.getEntities());
}

void ScenePanelImpl::onInstanceSelection(wxEvent&)
{
  auto index = m_listBox->GetSelection();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_listBox->GetClientData(index));
  m_mode.selectEntity(entity.id, entity.type);

  m_componentsPanel->onEntitySelect(entity.id);
}

wxPanel* ScenePanelImpl::getWxPtr()
{
  return m_basePanel;
}

void ScenePanelImpl::populate(const std::vector<EntityIdAndType>& entities)
{
  m_listBox->Clear();

  for (size_t i = 0; i < entities.size(); ++i) {
    m_listBox->Insert(STR("[" << entities[i].id << "] " << entities[i].type), i,
      new EntityIdAndType{entities[i]});
  }
}

} // namespace

ScenePanelPtr createScenePanel(wxWindow* parent, EditorCore& core, SceneEditMode& mode)
{
  return std::make_unique<ScenePanelImpl>(parent, core, mode);
}

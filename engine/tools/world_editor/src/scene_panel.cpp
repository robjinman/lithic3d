#include "scene_panel.hpp"
#include "world_editor.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

class ScenePanelImpl : public ScenePanel
{
  public:
    ScenePanelImpl(wxWindow* parent, WorldEditor& worldEditor);

    void populate(const std::vector<EntityIdAndType>& entities) override;
    wxPanel* getWxPtr() override;

  private:
    void onInstanceSelection(wxEvent& e);
    void onCancelClick();
    void onApplyClick();

    WorldEditor& m_worldEditor;
    wxPanel* m_basePanel = nullptr;
    wxListBox* m_listBox = nullptr;

    void onAddOrRemoveEntity();
};

ScenePanelImpl::ScenePanelImpl(wxWindow* parent, WorldEditor& worldEditor)
  : m_worldEditor(worldEditor)
{
  m_basePanel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_basePanel->SetSizer(vbox);

  m_listBox = new wxListBox{m_basePanel, wxID_ANY};
  vbox->Add(m_listBox, 1, wxEXPAND | wxALL);

  wxButton* btnCancel = new wxButton(m_basePanel, wxID_ANY, "Cancel");
  wxButton* btnApply = new wxButton(m_basePanel, wxID_ANY, "Apply");

  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(btnCancel);
  hbox->Add(btnApply);

  vbox->Add(hbox);

  btnCancel->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCancelClick(); });
  btnApply->Bind(wxEVT_BUTTON, [this](wxEvent&) { onApplyClick(); });

  m_listBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent& e) { onInstanceSelection(e); });

  m_worldEditor.listen(WorldEditor::Event::AddOrRemoveEntity, [this]() { onAddOrRemoveEntity(); });
}

void ScenePanelImpl::onAddOrRemoveEntity()
{
  populate(m_worldEditor.getEntities());
}

void ScenePanelImpl::onInstanceSelection(wxEvent&)
{
  auto index = m_listBox->GetSelection();
  if (index == wxNOT_FOUND) {
    return;
  }

  auto& entity = *reinterpret_cast<EntityIdAndType*>(m_listBox->GetClientData(index));
  m_worldEditor.selectEntity(entity.id, entity.type);
}

void ScenePanelImpl::onCancelClick()
{
  m_worldEditor.cancelTransform();
}

void ScenePanelImpl::onApplyClick()
{
  m_worldEditor.applyTransform();
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

ScenePanelPtr createScenePanel(wxWindow* parent, WorldEditor& worldEditor)
{
  return std::make_unique<ScenePanelImpl>(parent, worldEditor);
}

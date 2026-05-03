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

    void populate(const std::vector<Entity>& entities) override;
    wxPanel* getWxPtr() override;

  private:
    void onInstanceSelection(wxEvent& e);

    WorldEditor& m_worldEditor;
    wxPanel* m_basePanel = nullptr;
    wxListBox* m_listBox = nullptr;
};

ScenePanelImpl::ScenePanelImpl(wxWindow* parent, WorldEditor& worldEditor)
  : m_worldEditor(worldEditor)
{
  m_basePanel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_basePanel->SetSizer(vbox);

  m_listBox = new wxListBox{m_basePanel, wxID_ANY};
  vbox->Add(m_listBox, 1, wxEXPAND | wxALL);

  m_listBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent& e) { onInstanceSelection(e); });
}

void ScenePanelImpl::onInstanceSelection(wxEvent&)
{
  auto selected = m_listBox->GetStringSelection().ToStdString();
  // TODO
}

wxPanel* ScenePanelImpl::getWxPtr()
{
  return m_basePanel;
}

void ScenePanelImpl::populate(const std::vector<Entity>& entities)
{
  for (size_t i = 0; i < entities.size(); ++i) {
     m_listBox->Insert(STR("[" << entities[i].id << "] " << entities[i].type), i);
  }
}

} // namespace

ScenePanelPtr createScenePanel(wxWindow* parent, WorldEditor& worldEditor)
{
  return std::make_unique<ScenePanelImpl>(parent, worldEditor);
}

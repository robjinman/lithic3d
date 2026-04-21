#include "prefabs_panel.hpp"
#include "world_editor.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

class PrefabsPanelImpl : public PrefabsPanel
{
  public:
    PrefabsPanelImpl(wxWindow* parent, WorldEditor& worldEditor);

    void populate() override;
    wxPanel* getWxPtr() override;

  private:
    void createInstancePanel();
    void onPrefabSelection(wxEvent& e);

    WorldEditor& m_worldEditor;
    wxPanel* m_basePanel = nullptr;
    wxListBox* m_listBox = nullptr;
    wxPanel* m_instancePanel = nullptr;
};

PrefabsPanelImpl::PrefabsPanelImpl(wxWindow* parent, WorldEditor& worldEditor)
  : m_worldEditor(worldEditor)
{
  m_basePanel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_basePanel->SetSizer(vbox);

  m_listBox = new wxListBox{m_basePanel, wxID_ANY};
  vbox->Add(m_listBox, 1, wxEXPAND | wxALL);

  m_listBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent& e) { onPrefabSelection(e); });

  createInstancePanel();
}

void PrefabsPanelImpl::onPrefabSelection(wxEvent&)
{
  auto selected = m_listBox->GetStringSelection().ToStdString();
  m_worldEditor.instantiatePrefabAtCursor(selected);
}

void PrefabsPanelImpl::createInstancePanel()
{
  m_instancePanel = new wxPanel(m_basePanel, wxID_ANY);
  m_basePanel->GetSizer()->Add(m_instancePanel, 1, wxEXPAND | wxALL);

  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_instancePanel->SetSizer(vbox);

  wxButton* btnCreateInstance = new wxButton(m_instancePanel, wxID_ANY, "Create");
  vbox->Add(btnCreateInstance);
}

wxPanel* PrefabsPanelImpl::getWxPtr()
{
  return m_basePanel;
}

void PrefabsPanelImpl::populate()
{
  auto prefabNames = m_worldEditor.listPrefabs();

  for (size_t i = 0; i < prefabNames.size(); ++i) {
     m_listBox->Insert(prefabNames[i], i);
  }
}

} // namespace

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, WorldEditor& worldEditor)
{
  return std::make_unique<PrefabsPanelImpl>(parent, worldEditor);
}

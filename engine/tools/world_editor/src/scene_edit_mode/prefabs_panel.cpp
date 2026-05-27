#include "scene_edit_mode/prefabs_panel.hpp"
#include "scene_edit_mode/scene_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>

using namespace lithic3d;
namespace fs = std::filesystem;

namespace
{

class PrefabsPanelImpl : public PrefabsPanel
{
  public:
    PrefabsPanelImpl(wxWindow* parent, EditorCore& editorCore, SceneEditMode& mode);

    void populate() override;
    wxPanel* getWxPtr() override;

  private:
    void onPrefabSelection(wxEvent& e);
    void onCancelClick();
    void onCreateClick();

    EditorCore& m_core;
    SceneEditMode& m_mode;
    wxPanel* m_basePanel = nullptr;
    wxListBox* m_listBox = nullptr;
};

PrefabsPanelImpl::PrefabsPanelImpl(wxWindow* parent, EditorCore& editorCore, SceneEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
{
  m_basePanel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_basePanel->SetSizer(vbox);

  m_listBox = new wxListBox{m_basePanel, wxID_ANY};
  vbox->Add(m_listBox, wxSizerFlags(1).Expand());

  m_listBox->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, [this](wxEvent& e) { onPrefabSelection(e); });

  wxButton* btnCancel = new wxButton(m_basePanel, wxID_ANY, "Cancel");
  wxButton* btnCreate = new wxButton(m_basePanel, wxID_ANY, "Create");

  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(btnCancel);
  hbox->Add(btnCreate);

  vbox->Add(hbox);

  btnCancel->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCancelClick(); });
  btnCreate->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCreateClick(); });
}

void PrefabsPanelImpl::onPrefabSelection(wxEvent&)
{
  auto selected = m_listBox->GetStringSelection().ToStdString();

  if (!selected.empty()) {
    m_mode.setActivePrefab(selected);
  }
}

void PrefabsPanelImpl::onCancelClick()
{
  m_listBox->DeselectAll();
  m_mode.cancelActivePrefab();
}

void PrefabsPanelImpl::onCreateClick()
{
  m_listBox->DeselectAll();
  m_mode.instantiateActivePrefab();
}

wxPanel* PrefabsPanelImpl::getWxPtr()
{
  return m_basePanel;
}

void PrefabsPanelImpl::populate()
{
  auto prefabNames = m_core.listPrefabs();

  for (size_t i = 0; i < prefabNames.size(); ++i) {
     m_listBox->Insert(prefabNames[i], i);
  }
}

} // namespace

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, EditorCore& editorCore, SceneEditMode& mode)
{
  return std::make_unique<PrefabsPanelImpl>(parent, editorCore, mode);
}

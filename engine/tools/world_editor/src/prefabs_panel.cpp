#include "prefabs_panel.hpp"
#include "world_editor.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

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
    void onCancelClick();
    void onCreateClick();
    void onDistanceChange(wxEvent& e);
    void onScaleChange(wxEvent& e);
    void onRotationChange(wxEvent& e);

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
  m_worldEditor.setActivePrefab(selected);
}

void PrefabsPanelImpl::createInstancePanel()
{
  m_instancePanel = new wxPanel(m_basePanel, wxID_ANY);
  m_basePanel->GetSizer()->Add(m_instancePanel, 1, wxEXPAND | wxALL);

  auto grid = new wxGridBagSizer(0, 0);
  m_instancePanel->SetSizer(grid);

  wxStaticText* lblDistance = new wxStaticText(m_instancePanel, wxID_ANY, "Distance (metres)");

  wxSpinCtrlDouble* spnDistance = new wxSpinCtrlDouble(m_instancePanel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1.0, 100.0, 10.0, 0.1);

  wxStaticText* lblScale = new wxStaticText(m_instancePanel, wxID_ANY, "Scale");

  wxSpinCtrlDouble* spnScale = new wxSpinCtrlDouble(m_instancePanel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  wxStaticText* lblRotation = new wxStaticText(m_instancePanel, wxID_ANY, "Rotation");

  wxSlider* sldRotation = new wxSlider(m_instancePanel, wxID_ANY, 0, -180, 180, wxDefaultPosition,
    wxDefaultSize, wxSL_LABELS);

  wxButton* btnCancel = new wxButton(m_instancePanel, wxID_ANY, "Cancel");
  wxButton* btnCreate = new wxButton(m_instancePanel, wxID_ANY, "Create");

  grid->Add(btnCancel, wxGBPosition(0, 0));
  grid->Add(btnCreate, wxGBPosition(0, 1));

  grid->Add(lblDistance, wxGBPosition(1, 0));
  grid->Add(spnDistance, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND);

  grid->Add(lblScale, wxGBPosition(2, 0));
  grid->Add(spnScale, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND);

  grid->Add(lblRotation, wxGBPosition(3, 0));
  grid->Add(sldRotation, wxGBPosition(4, 0), wxGBSpan(1, 2), wxEXPAND);

  grid->AddGrowableCol(1, 1);

  btnCancel->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCancelClick(); });
  btnCreate->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCreateClick(); });
  spnDistance->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onDistanceChange(e); });
  spnScale->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onScaleChange(e); });
  sldRotation->Bind(wxEVT_SLIDER, [this](wxEvent& e) { onRotationChange(e); });
}

void PrefabsPanelImpl::onDistanceChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  m_worldEditor.setCursorDistance(event.GetValue());
}

void PrefabsPanelImpl::onScaleChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  m_worldEditor.setCursorScale(event.GetValue());
}

void PrefabsPanelImpl::onRotationChange(wxEvent& e)
{
  auto event = dynamic_cast<wxCommandEvent&>(e);
  m_worldEditor.setCursorRotation(Vec3f{ 0.f,
    degreesToRadians(static_cast<float>(event.GetInt())), 0.f });
}

void PrefabsPanelImpl::onCancelClick()
{
  m_listBox->DeselectAll();
  m_worldEditor.cancelActivePrefab();
}

void PrefabsPanelImpl::onCreateClick()
{
  m_listBox->DeselectAll();
  m_worldEditor.instantiateActivePrefab();
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

#include "transform_panel.hpp"
#include "world_editor.hpp"
#include <lithic3d/utils.hpp>
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

using namespace lithic3d;

namespace
{

class TransformPanelImpl : public TransformPanel
{
  public:
    TransformPanelImpl(wxWindow* parent, WorldEditor& worldEditor);

    wxPanel* getWxPtr() override;

  private:
    void onDistanceChange(wxEvent& e);
    void onScaleChange(wxEvent& e);
    void onRotationChange(wxEvent& e);

    wxPanel* m_panel;
    WorldEditor& m_worldEditor;
};

TransformPanelImpl::TransformPanelImpl(wxWindow* parent, WorldEditor& worldEditor)
  : m_worldEditor(worldEditor)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto grid = new wxGridBagSizer(0, 0);
  m_panel->SetSizer(grid);

  wxStaticText* lblDistance = new wxStaticText(m_panel, wxID_ANY, "Distance (metres)");

  wxSpinCtrlDouble* spnDistance = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1.0, 100.0, 10.0, 0.1);

  wxStaticText* lblScale = new wxStaticText(m_panel, wxID_ANY, "Scale");

  wxSpinCtrlDouble* spnScale = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  wxStaticText* lblRotation = new wxStaticText(m_panel, wxID_ANY, "Rotation");

  wxSlider* sldRotation = new wxSlider(m_panel, wxID_ANY, 0, -180, 180, wxDefaultPosition,
    wxDefaultSize, wxSL_LABELS);

  grid->Add(lblDistance, wxGBPosition(0, 0));
  grid->Add(spnDistance, wxGBPosition(0, 1), wxDefaultSpan, wxEXPAND);

  grid->Add(lblScale, wxGBPosition(1, 0));
  grid->Add(spnScale, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND);

  grid->Add(lblRotation, wxGBPosition(2, 0));
  grid->Add(sldRotation, wxGBPosition(3, 0), wxGBSpan(1, 2), wxEXPAND);

  grid->AddGrowableCol(1, 1);

  spnDistance->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onDistanceChange(e); });
  spnScale->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onScaleChange(e); });
  sldRotation->Bind(wxEVT_SLIDER, [this](wxEvent& e) { onRotationChange(e); });
}

wxPanel* TransformPanelImpl::getWxPtr()
{
  return m_panel;
}

void TransformPanelImpl::onDistanceChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  m_worldEditor.setCursorDistance(event.GetValue());
}

void TransformPanelImpl::onScaleChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  m_worldEditor.setCursorScale(event.GetValue());
}

void TransformPanelImpl::onRotationChange(wxEvent& e)
{
  auto event = dynamic_cast<wxCommandEvent&>(e);
  m_worldEditor.setCursorRotation(Vec3f{ 0.f,
    degreesToRadians(static_cast<float>(event.GetInt())), 0.f });
}

} // namespace

TransformPanelPtr createTransformPanel(wxWindow* parent, WorldEditor& worldEditor)
{
  return std::make_unique<TransformPanelImpl>(parent, worldEditor);
}

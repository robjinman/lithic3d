#include "current_transform_panel.hpp"
#include "editor_core.hpp"
#include <lithic3d/utils.hpp>
#include <lithic3d/units.hpp>
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

using namespace lithic3d;

namespace
{

class CurrentTransformPanelImpl : public CurrentTransformPanel
{
  public:
    CurrentTransformPanelImpl(wxWindow* parent, EditorCore& editorCore);

    wxWindow* getWxPtr() override;

  private:
    void onDistanceChange(wxEvent& e);
    void onScaleChange(wxEvent& e);
    void onRotationChange(wxEvent& e);
    void onCursorMove();

    wxWindow* m_window = nullptr;
    EditorCore& m_core;
    wxSpinCtrlDouble* m_spnDistance = nullptr;
    wxSpinCtrlDouble* m_spnScale = nullptr;
    wxSlider* m_sldEulerY = nullptr;
    EventHandle m_onCursorMove;
};

CurrentTransformPanelImpl::CurrentTransformPanelImpl(wxWindow* parent, EditorCore& editorCore)
  : m_core(editorCore)
{
  m_window = new wxPanel(parent, wxID_ANY);
  //auto vbox = new wxBoxSizer(wxVERTICAL);

  auto staticBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_window, "Cursor");
  auto staticBox = staticBoxSizer->GetStaticBox();

  auto grid = new wxGridBagSizer(5, 5);

  wxStaticText* lblDistance = new wxStaticText(staticBox, wxID_ANY, "Distance (metres)");

  m_spnDistance = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1.0, 100.0, 10.0, 0.1);

  wxStaticText* lblScale = new wxStaticText(staticBox, wxID_ANY, "Scale");

  m_spnScale = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  wxStaticText* lblRotation = new wxStaticText(staticBox, wxID_ANY, "Rotation");

  m_sldEulerY = new wxSlider(staticBox, wxID_ANY, 0, -180, 180, wxDefaultPosition,
    wxDefaultSize, wxSL_LABELS);

  int border = 10;

  grid->Add(lblDistance, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_spnDistance, wxGBPosition(0, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);

  grid->Add(lblScale, wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT, border);
  grid->Add(m_spnScale, wxGBPosition(1, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);

  grid->Add(lblRotation, wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_sldEulerY, wxGBPosition(3, 0), wxGBSpan(1, 2), wxEXPAND | wxLEFT | wxRIGHT, border);

  grid->AddGrowableCol(1);

  staticBox->SetSizer(grid);

  //vbox->Add(staticBox, wxSizerFlags(1).Expand());

  m_window->SetSizer(staticBoxSizer);

  m_spnDistance->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onDistanceChange(e); });
  m_spnScale->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onScaleChange(e); });
  m_sldEulerY->Bind(wxEVT_SLIDER, [this](wxEvent& e) { onRotationChange(e); });

  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

wxWindow* CurrentTransformPanelImpl::getWxPtr()
{
  return m_window;
}

void CurrentTransformPanelImpl::onCursorMove()
{
  m_spnDistance->SetValue(worldUnitsToMetres(m_core.getCursorDistance()));
  m_spnScale->SetValue(m_core.getCursorScale()[0] / WORLD_UNITS_PER_METRE);  // TODO

  float radiansY = m_core.getCursorRotation()[1];
  int degreesY = static_cast<int>(radiansToDegrees(radiansY) + 0.5f);
  assert(inRange(degreesY, -180, 180));
  m_sldEulerY->SetValue(degreesY);
}

void CurrentTransformPanelImpl::onDistanceChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);

  m_onCursorMove.reset();
  m_core.setCursorDistance(metresToWorldUnits(event.GetValue()));
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CurrentTransformPanelImpl::onScaleChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  Vec3f scale = Vec3f{ 1.f, 1.f, 1.f } * event.GetValue();

  m_onCursorMove.reset();
  m_core.setCursorScale(scale * WORLD_UNITS_PER_METRE);
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CurrentTransformPanelImpl::onRotationChange(wxEvent& e)
{
  auto event = dynamic_cast<wxCommandEvent&>(e);

  m_onCursorMove.reset();
  m_core.setCursorRotation(Vec3f{ 0.f, degreesToRadians(static_cast<float>(event.GetInt())), 0.f });
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

} // namespace

CurrentTransformPanelPtr createCurrentTransformPanel(wxWindow* parent, EditorCore& editorCore)
{
  return std::make_unique<CurrentTransformPanelImpl>(parent, editorCore);
}

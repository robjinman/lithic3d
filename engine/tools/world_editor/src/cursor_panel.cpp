#include "cursor_panel.hpp"
#include "editor_core.hpp"
#include <lithic3d/utils.hpp>
#include <lithic3d/units.hpp>
#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>

using namespace lithic3d;

wxDEFINE_EVENT(ECancelActiveTransform, wxCommandEvent);
wxDEFINE_EVENT(EApplyActiveTransform, wxCommandEvent);

namespace
{

class CursorPanelImpl : public CursorPanel
{
  public:
    CursorPanelImpl(wxWindow* parent, EditorCore& editorCore);

    wxWindow* getWxPtr() override;

  private:
    void onDistanceChange(wxEvent& e);
    void onScaleChange(wxEvent& e);
    void onRotationChange(wxEvent& e);
    void onCursorMove();
    void onCancelClick();
    void onApplyClick();

    wxWindow* m_window = nullptr;
    EditorCore& m_core;
    wxSpinCtrlDouble* m_spnDistance = nullptr;
    wxSpinCtrlDouble* m_spnScale = nullptr;
    wxSlider* m_sldEulerY = nullptr;
    EventHandle m_onCursorMove;
};

CursorPanelImpl::CursorPanelImpl(wxWindow* parent, EditorCore& editorCore)
  : m_core(editorCore)
{
  m_window = new wxPanel(parent, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);

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

  vbox->Add(grid, wxSizerFlags().Expand());

  wxButton* btnCancel = new wxButton(staticBox, wxID_ANY, "Cancel");
  wxButton* btnApply = new wxButton(staticBox, wxID_ANY, "Apply");

  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(btnCancel, wxSizerFlags(1).Expand().Border(wxLEFT, border));
  hbox->Add(btnApply, wxSizerFlags(1).Expand().Border(wxRIGHT, border));

  vbox->Add(hbox, wxSizerFlags().Expand());

  staticBox->SetSizer(vbox);

  m_window->SetSizer(staticBoxSizer);

  m_spnDistance->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onDistanceChange(e); });
  m_spnScale->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent& e) { onScaleChange(e); });
  m_sldEulerY->Bind(wxEVT_SLIDER, [this](wxEvent& e) { onRotationChange(e); });

  btnCancel->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCancelClick(); });
  btnApply->Bind(wxEVT_BUTTON, [this](wxEvent&) { onApplyClick(); });

  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CursorPanelImpl::onCancelClick()
{
  wxCommandEvent event(ECancelActiveTransform);
  wxPostEvent(m_window, event);
}

void CursorPanelImpl::onApplyClick()
{
  wxCommandEvent event(EApplyActiveTransform);
  wxPostEvent(m_window, event);
}

wxWindow* CursorPanelImpl::getWxPtr()
{
  return m_window;
}

void CursorPanelImpl::onCursorMove()
{
  m_spnDistance->SetValue(worldUnitsToMetres(m_core.getCursorDistance()));
  m_spnScale->SetValue(m_core.getCursorScale()[0]);

  float radiansY = m_core.getCursorRotation()[1];
  int degreesY = static_cast<int>(radiansToDegrees(radiansY) + 0.5f);
  assert(inRange(degreesY, -180, 180));
  m_sldEulerY->SetValue(degreesY);
}

void CursorPanelImpl::onDistanceChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);

  m_onCursorMove.reset();
  m_core.setCursorDistance(metresToWorldUnits(event.GetValue()));
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CursorPanelImpl::onScaleChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);
  Vec3f scale = Vec3f{ 1.f, 1.f, 1.f } * event.GetValue();

  m_onCursorMove.reset();
  m_core.setCursorScale(scale);
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CursorPanelImpl::onRotationChange(wxEvent& e)
{
  auto event = dynamic_cast<wxCommandEvent&>(e);

  m_onCursorMove.reset();
  m_core.setCursorRotation(Vec3f{ 0.f, degreesToRadians(static_cast<float>(event.GetInt())), 0.f });
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

} // namespace

CursorPanelPtr createCursorPanel(wxWindow* parent, EditorCore& editorCore)
{
  return std::make_unique<CursorPanelImpl>(parent, editorCore);
}

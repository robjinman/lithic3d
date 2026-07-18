#include "cursor_panel.hpp"
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
    void onScaleChange();
    void onRotationChange();
    void onCursorMove();
    void onCancelClick();
    void onApplyClick();

    wxWindow* m_window = nullptr;
    EditorCore& m_core;
    wxSpinCtrlDouble* m_spnDistance = nullptr;
    // TODO: Lock ratio checkbox
    wxSpinCtrlDouble* m_spnScaleX = nullptr;
    wxSpinCtrlDouble* m_spnScaleY = nullptr;
    wxSpinCtrlDouble* m_spnScaleZ = nullptr;
    wxSlider* m_sldEulerX = nullptr;
    wxSlider* m_sldEulerY = nullptr;
    wxSlider* m_sldEulerZ = nullptr;
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

  m_spnDistance = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString, wxDefaultPosition,
    wxDefaultSize, wxSP_ARROW_KEYS, MIN_CURSOR_DISTANCE, MAX_CURSOR_DISTANCE, 10.0, 0.1);

  wxStaticText* lblScaleX = new wxStaticText(staticBox, wxID_ANY, "Scale X");
  wxStaticText* lblScaleY = new wxStaticText(staticBox, wxID_ANY, "Scale Y");
  wxStaticText* lblScaleZ = new wxStaticText(staticBox, wxID_ANY, "Scale Z");

  m_spnScaleX = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString, wxDefaultPosition,
    wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  m_spnScaleY = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString, wxDefaultPosition,
    wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  m_spnScaleZ = new wxSpinCtrlDouble(staticBox, wxID_ANY, wxEmptyString, wxDefaultPosition,
    wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 100.0, 1.0, 0.01);

  wxStaticText* lblRotation = new wxStaticText(staticBox, wxID_ANY, "Rotation");

  m_sldEulerX = new wxSlider(staticBox, wxID_ANY, 0, -180, 180, wxDefaultPosition, wxDefaultSize,
    wxSL_LABELS);

  m_sldEulerY = new wxSlider(staticBox, wxID_ANY, 0, -180, 180, wxDefaultPosition, wxDefaultSize,
    wxSL_LABELS);

  m_sldEulerZ = new wxSlider(staticBox, wxID_ANY, 0, -180, 180, wxDefaultPosition, wxDefaultSize,
    wxSL_LABELS);

  int border = 10;

  int row = 0;
  grid->Add(lblDistance, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_spnDistance, wxGBPosition(row, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);
  ++row;

  grid->Add(lblScaleX, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_spnScaleX, wxGBPosition(row, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);
  ++row;

  grid->Add(lblScaleY, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_spnScaleY, wxGBPosition(row, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);
  ++row;

  grid->Add(lblScaleZ, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  grid->Add(m_spnScaleZ, wxGBPosition(row, 1), wxDefaultSpan, wxEXPAND | wxRIGHT, border);
  ++row;

  grid->Add(lblRotation, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT,
    border);
  ++row;

  grid->Add(m_sldEulerX, wxGBPosition(row, 0), wxGBSpan(1, 2), wxEXPAND | wxLEFT | wxRIGHT, border);
  ++row;

  grid->Add(m_sldEulerY, wxGBPosition(row, 0), wxGBSpan(1, 2), wxEXPAND | wxLEFT | wxRIGHT, border);
  ++row;

  grid->Add(m_sldEulerZ, wxGBPosition(row, 0), wxGBSpan(1, 2), wxEXPAND | wxLEFT | wxRIGHT, border);
  ++row;

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
  m_spnScaleX->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent&) { onScaleChange(); });
  m_spnScaleY->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent&) { onScaleChange(); });
  m_spnScaleZ->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxEvent&) { onScaleChange(); });
  m_sldEulerX->Bind(wxEVT_SLIDER, [this](wxEvent&) { onRotationChange(); });
  m_sldEulerY->Bind(wxEVT_SLIDER, [this](wxEvent&) { onRotationChange(); });
  m_sldEulerZ->Bind(wxEVT_SLIDER, [this](wxEvent&) { onRotationChange(); });

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
  m_spnScaleX->SetValue(m_core.getCursorScale()[0]);
  m_spnScaleY->SetValue(m_core.getCursorScale()[1]);
  m_spnScaleZ->SetValue(m_core.getCursorScale()[2]);

  auto updateSlider = [this](uint32_t dimension, wxSlider* slider) {
    float radians = m_core.getCursorRotation()[dimension];
    int degrees = static_cast<int>(radiansToDegrees(radians) + 0.5f);
    assert(inRange(degrees, -180, 180));
    slider->SetValue(degrees);
  };

  updateSlider(0, m_sldEulerX);
  updateSlider(1, m_sldEulerY);
  updateSlider(2, m_sldEulerZ);
}

void CursorPanelImpl::onDistanceChange(wxEvent& e)
{
  auto event = dynamic_cast<wxSpinDoubleEvent&>(e);

  m_onCursorMove.reset();
  m_core.setCursorDistance(metresToWorldUnits(event.GetValue()));
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CursorPanelImpl::onScaleChange()
{
  Vec3f scale = {
    static_cast<float>(m_spnScaleX->GetValue()),
    static_cast<float>(m_spnScaleY->GetValue()),
    static_cast<float>(m_spnScaleZ->GetValue())
  };

  m_onCursorMove.reset();
  m_core.setCursorScale(scale);
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

void CursorPanelImpl::onRotationChange()
{
  Vec3f rotation{
    degreesToRadians(static_cast<float>(m_sldEulerX->GetValue())),
    degreesToRadians(static_cast<float>(m_sldEulerY->GetValue())),
    degreesToRadians(static_cast<float>(m_sldEulerZ->GetValue()))
  };

  m_onCursorMove.reset();
  m_core.setCursorRotation(rotation);
  m_onCursorMove = m_core.listen(EditorCore::Event::CursorMove, [this]() { onCursorMove(); });
}

} // namespace

CursorPanelPtr createCursorPanel(wxWindow* parent, EditorCore& editorCore)
{
  return std::make_unique<CursorPanelImpl>(parent, editorCore);
}

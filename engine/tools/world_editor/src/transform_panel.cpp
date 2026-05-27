#include "transform_panel.hpp"
#include <lithic3d/strings.hpp>
#include <wx/wx.h>

using namespace lithic3d;

namespace
{

class TransformPanelImpl : public TransformPanel
{
  public:
    TransformPanelImpl(wxWindow* parent);

    wxWindow* getWxPtr() override;
    void setTransform(const Mat4x4f& transform) override;

  private:
    wxPanel* m_panel = nullptr;
    wxTextCtrl* m_txtMatrix = nullptr;
};

TransformPanelImpl::TransformPanelImpl(wxWindow* parent)
{
  m_panel = new wxPanel(parent);

  auto grid = new wxFlexGridSizer(2);
  auto lblMatrix = new wxStaticText(m_panel, wxID_ANY, "Matrix");

  m_txtMatrix = new wxTextCtrl(m_panel, wxID_ANY, STR(identityMatrix<4>()));

  grid->Add(lblMatrix, wxSizerFlags().CentreVertical());
  grid->Add(m_txtMatrix, wxSizerFlags(1).Expand());

  grid->AddGrowableCol(1);

  m_panel->SetSizer(grid);
  m_panel->Layout();
}

wxWindow* TransformPanelImpl::getWxPtr()
{
  return m_panel;
}

void TransformPanelImpl::setTransform(const Mat4x4f& transform)
{

}

} // namespace

TransformPanelPtr createTransformPanel(wxWindow* parent)
{
  return std::make_unique<TransformPanelImpl>(parent);
}

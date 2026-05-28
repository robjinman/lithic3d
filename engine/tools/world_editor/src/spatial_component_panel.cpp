#include "component_panel.hpp"
#include "editor_core.hpp"
#include "transform_panel.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>

using namespace lithic3d;

namespace
{

class AabbPanel
{
  public:
    explicit AabbPanel(wxWindow* parent);

    wxWindow* getWxPtr();
    bool hasChanges() const;
    void setAabb(const Aabb& aabb);

  private:
    wxPanel* m_panel = nullptr;
    wxTextCtrl* m_txtXMin = nullptr;
    wxTextCtrl* m_txtXMax = nullptr;
    wxTextCtrl* m_txtYMin = nullptr;
    wxTextCtrl* m_txtYMax = nullptr;
    wxTextCtrl* m_txtZMin = nullptr;
    wxTextCtrl* m_txtZMax = nullptr;
    bool m_hasChanges = false;
};

AabbPanel::AabbPanel(wxWindow* parent)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto grid = new wxFlexGridSizer(4);

  auto lblXMin = new wxStaticText(m_panel, wxID_ANY, "Min X");
  m_txtXMin = new wxTextCtrl(m_panel, wxID_ANY);
  auto lblXMax = new wxStaticText(m_panel, wxID_ANY, "Max X");
  m_txtXMax = new wxTextCtrl(m_panel, wxID_ANY);

  auto lblYMin = new wxStaticText(m_panel, wxID_ANY, "Min Y");
  m_txtYMin = new wxTextCtrl(m_panel, wxID_ANY);
  auto lblYMax = new wxStaticText(m_panel, wxID_ANY, "Max Y");
  m_txtYMax = new wxTextCtrl(m_panel, wxID_ANY);

  auto lblZMin = new wxStaticText(m_panel, wxID_ANY, "Min Z");
  m_txtZMin = new wxTextCtrl(m_panel, wxID_ANY);
  auto lblZMax = new wxStaticText(m_panel, wxID_ANY, "Max Z");
  m_txtZMax = new wxTextCtrl(m_panel, wxID_ANY);

  grid->Add(lblXMin, wxSizerFlags().CentreVertical());
  grid->Add(m_txtXMin, wxSizerFlags().Expand());
  grid->Add(lblXMax, wxSizerFlags().CentreVertical());
  grid->Add(m_txtXMax, wxSizerFlags().Expand());

  grid->Add(lblYMin, wxSizerFlags().CentreVertical());
  grid->Add(m_txtYMin, wxSizerFlags().Expand());
  grid->Add(lblYMax, wxSizerFlags().CentreVertical());
  grid->Add(m_txtYMax, wxSizerFlags().Expand());

  grid->Add(lblZMin, wxSizerFlags().CentreVertical());
  grid->Add(m_txtZMin, wxSizerFlags().Expand());
  grid->Add(lblZMax, wxSizerFlags().CentreVertical());
  grid->Add(m_txtZMax, wxSizerFlags().Expand());

  grid->AddGrowableCol(1);
  grid->AddGrowableCol(3);

  m_panel->SetSizer(grid);

  m_txtXMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtXMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtYMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtYMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtZMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtZMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
}

bool AabbPanel::hasChanges() const
{
  return m_hasChanges;
}

wxWindow* AabbPanel::getWxPtr()
{
  return m_panel;
}

void AabbPanel::setAabb(const Aabb& aabb)
{
  m_txtXMin->SetValue(std::to_string(aabb.min[0]));
  m_txtXMax->SetValue(std::to_string(aabb.max[0]));
  m_txtYMin->SetValue(std::to_string(aabb.min[1]));
  m_txtYMax->SetValue(std::to_string(aabb.max[1]));
  m_txtZMin->SetValue(std::to_string(aabb.min[2]));
  m_txtZMax->SetValue(std::to_string(aabb.max[2]));

  m_hasChanges = false;
}

class SpatialComponentPanel : public ComponentPanel
{
  public:
    SpatialComponentPanel(wxWindow* parent, EditorCore& core);

    wxPanel* getWxPtr() override;
    void populate(EntityId entityId) override;
    bool hasChanges() const override;

  private:
    EditorCore& m_core;
    wxPanel* m_panel = nullptr;
    TransformPanelPtr m_transformPanel = nullptr;
    std::unique_ptr<AabbPanel> m_aabbPanel = nullptr;
};

SpatialComponentPanel::SpatialComponentPanel(wxWindow* parent, EditorCore& core)
  : m_core(core)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto transformBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Transform");
  m_transformPanel = createTransformPanel(transformBoxSizer->GetStaticBox());
  transformBoxSizer->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  auto aabbBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "AABB");
  m_aabbPanel = std::make_unique<AabbPanel>(aabbBoxSizer->GetStaticBox());
  aabbBoxSizer->Add(m_aabbPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(transformBoxSizer, wxSizerFlags(1).Expand());
  vbox->Add(aabbBoxSizer, wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);
  m_panel->Layout();
}

bool SpatialComponentPanel::hasChanges() const
{
  return m_aabbPanel->hasChanges();
}

void SpatialComponentPanel::populate(EntityId entityId)
{
  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  auto& transform = sysSpatial.getLocalTransform(entityId);
  auto& aabb = sysSpatial.getAabb(entityId);

  m_transformPanel->setTransform(transform);
  m_aabbPanel->setAabb(aabb);
}

wxPanel* SpatialComponentPanel::getWxPtr()
{
  return m_panel;
}

} // namespace

ComponentPanelPtr createSpatialComponentPanel(wxWindow* parent, EditorCore& core)
{
  return std::make_unique<SpatialComponentPanel>(parent, core);
}

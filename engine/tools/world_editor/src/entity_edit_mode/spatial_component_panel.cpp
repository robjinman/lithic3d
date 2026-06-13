#include "entity_edit_mode/component_panel.hpp"
#include "entity_edit_mode/entity_edit_mode.hpp"
#include "editor_core.hpp"
#include "transform_panel.hpp"
#include <lithic3d/lithic3d.hpp>
#include <wx/wx.h>
#include <wx/spinctrl.h>

using namespace lithic3d;

namespace
{

class AabbPanel
{
  public:
    AabbPanel(wxWindow* parent, EntityEditMode& mode);

    wxWindow* getWxPtr();
    bool hasChanges() const;
    void setAabb(const Aabb& aabb, bool resetDirtyFlag = true);

  private:
    EntityEditMode& m_mode;
    wxPanel* m_panel = nullptr;
    wxCheckBox* m_chkRender = nullptr;
    wxSpinCtrlDouble* m_spnXMin = nullptr;
    wxSpinCtrlDouble* m_spnXMax = nullptr;
    wxSpinCtrlDouble* m_spnYMin = nullptr;
    wxSpinCtrlDouble* m_spnYMax = nullptr;
    wxSpinCtrlDouble* m_spnZMin = nullptr;
    wxSpinCtrlDouble* m_spnZMax = nullptr;
    bool m_hasChanges = false;

    void onChange();
    void onRenderToggle();
    Aabb getAabb() const;
};

AabbPanel::AabbPanel(wxWindow* parent, EntityEditMode& mode)
  : m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_chkRender = new wxCheckBox(m_panel, wxID_ANY, "Render");
  vbox->Add(m_chkRender, wxSizerFlags().Expand());

  auto grid = new wxFlexGridSizer(4);

  auto lblXMin = new wxStaticText(m_panel, wxID_ANY, "Min X");
  m_spnXMin = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);
  auto lblXMax = new wxStaticText(m_panel, wxID_ANY, "Max X");
  m_spnXMax = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);
  auto lblYMin = new wxStaticText(m_panel, wxID_ANY, "Min Y");
  m_spnYMin = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);
  auto lblYMax = new wxStaticText(m_panel, wxID_ANY, "Max Y");
  m_spnYMax = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);
  auto lblZMin = new wxStaticText(m_panel, wxID_ANY, "Min Z");
  m_spnZMin = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);
  auto lblZMax = new wxStaticText(m_panel, wxID_ANY, "Max Z");
  m_spnZMax = new wxSpinCtrlDouble(m_panel, wxID_ANY, wxEmptyString,
    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -100.0, 100.0, 1.0, 0.1);

  grid->Add(lblXMin, wxSizerFlags().CentreVertical());
  grid->Add(m_spnXMin, wxSizerFlags().Expand());
  grid->Add(lblXMax, wxSizerFlags().CentreVertical());
  grid->Add(m_spnXMax, wxSizerFlags().Expand());

  grid->Add(lblYMin, wxSizerFlags().CentreVertical());
  grid->Add(m_spnYMin, wxSizerFlags().Expand());
  grid->Add(lblYMax, wxSizerFlags().CentreVertical());
  grid->Add(m_spnYMax, wxSizerFlags().Expand());

  grid->Add(lblZMin, wxSizerFlags().CentreVertical());
  grid->Add(m_spnZMin, wxSizerFlags().Expand());
  grid->Add(lblZMax, wxSizerFlags().CentreVertical());
  grid->Add(m_spnZMax, wxSizerFlags().Expand());

  grid->AddGrowableCol(1);
  grid->AddGrowableCol(3);

  vbox->Add(grid, wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);

  m_chkRender->Bind(wxEVT_CHECKBOX, [this](wxEvent&) { onRenderToggle(); });

  m_spnXMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnXMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnYMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnYMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnZMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnZMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
}

void AabbPanel::onChange()
{
  m_hasChanges = true;
  m_mode.updateAabb(getAabb());
}

Aabb AabbPanel::getAabb() const
{
  return {
    .min = metresToWorldUnits(Vec3f{
      static_cast<float>(m_spnXMin->GetValue()),
      static_cast<float>(m_spnYMin->GetValue()),
      static_cast<float>(m_spnZMin->GetValue())
    }),
    .max = metresToWorldUnits(Vec3f{
      static_cast<float>(m_spnXMax->GetValue()),
      static_cast<float>(m_spnYMax->GetValue()),
      static_cast<float>(m_spnZMax->GetValue())
    })
  };
}

void AabbPanel::onRenderToggle()
{
  m_mode.renderAabb(m_chkRender->GetValue());
}

bool AabbPanel::hasChanges() const
{
  return m_hasChanges;
}

wxWindow* AabbPanel::getWxPtr()
{
  return m_panel;
}

void AabbPanel::setAabb(const Aabb& aabb, bool resetDirtyFlag)
{
  m_spnXMin->SetValue(std::to_string(worldUnitsToMetres(aabb.min[0])));
  m_spnXMax->SetValue(std::to_string(worldUnitsToMetres(aabb.max[0])));
  m_spnYMin->SetValue(std::to_string(worldUnitsToMetres(aabb.min[1])));
  m_spnYMax->SetValue(std::to_string(worldUnitsToMetres(aabb.max[1])));
  m_spnZMin->SetValue(std::to_string(worldUnitsToMetres(aabb.min[2])));
  m_spnZMax->SetValue(std::to_string(worldUnitsToMetres(aabb.max[2])));

  m_mode.updateAabb(getAabb());

  m_hasChanges = !resetDirtyFlag;
}

class SpatialComponentPanel : public ComponentPanel
{
  public:
    SpatialComponentPanel(wxWindow* parent, EditorCore& core, EntityEditMode& mode);

    wxPanel* getWxPtr() override;
    void populate(EntityId entityId) override;
    void repopulateFromMode() override;
    bool hasChanges() const override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    wxPanel* m_panel = nullptr;
    TransformPanelPtr m_transformPanel = nullptr;
    std::unique_ptr<AabbPanel> m_aabbPanel = nullptr;
    EntityId m_entityId = NULL_ENTITY_ID;
};

SpatialComponentPanel::SpatialComponentPanel(wxWindow* parent, EditorCore& core,
  EntityEditMode& mode)
  : m_core(core)
  , m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto transformBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Transform");
  m_transformPanel = createTransformPanel(transformBoxSizer->GetStaticBox());
  transformBoxSizer->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  auto aabbBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "AABB");
  m_aabbPanel = std::make_unique<AabbPanel>(aabbBoxSizer->GetStaticBox(), m_mode);
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
  m_entityId = entityId;

  auto& sysSpatial = m_core.engine().ecs().system<SysSpatial>();
  auto& transform = sysSpatial.getLocalTransform(m_entityId);
  auto& aabb = sysSpatial.getAabb(m_entityId);

  m_transformPanel->setTransform(transform);
  m_aabbPanel->setAabb(aabb);
}

void SpatialComponentPanel::repopulateFromMode()
{
  // Transform shouldn't change

  m_aabbPanel->setAabb(m_mode.getAabb(), false);
}

wxPanel* SpatialComponentPanel::getWxPtr()
{
  return m_panel;
}

} // namespace

ComponentPanelPtr createSpatialComponentPanel(wxWindow* parent, EditorCore& core,
  EntityEditMode& mode)
{
  return std::make_unique<SpatialComponentPanel>(parent, core, mode);
}

#include "component_panel.hpp"
#include "transform_panel.hpp"
#include "editor_core.hpp"
#include "tools.hpp"
#include <lithic3d/sys_collision.hpp>
#include <wx/tglbtn.h>

using namespace lithic3d;

namespace
{

class BoundingBoxPanel
{
  public:
    explicit BoundingBoxPanel(wxWindow* parent);

    wxWindow* getWxPtr();
    bool hasChanges() const;
    void populate(const BoundingBox& box);

  private:
    wxPanel* m_panel = nullptr;
    wxToggleButton* m_btnTool = nullptr;
    TransformPanelPtr m_transformPanel;
    wxTextCtrl* m_txtXMin = nullptr;
    wxTextCtrl* m_txtXMax = nullptr;
    wxTextCtrl* m_txtYMin = nullptr;
    wxTextCtrl* m_txtYMax = nullptr;
    wxTextCtrl* m_txtZMin = nullptr;
    wxTextCtrl* m_txtZMax = nullptr;
    bool m_hasChanges = false;

    void onToolToggle();
};

BoundingBoxPanel::BoundingBoxPanel(wxWindow* parent)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto transformBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Transform");
  m_btnTool = new wxToggleButton(transformBoxSizer->GetStaticBox(), wxID_ANY, "Edit");
  m_transformPanel = createTransformPanel(transformBoxSizer->GetStaticBox());
  transformBoxSizer->Add(m_btnTool);
  transformBoxSizer->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(transformBoxSizer, wxSizerFlags(1).Expand());

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

  vbox->Add(grid, wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);

  m_btnTool->Bind(wxEVT_TOGGLEBUTTON, [this](wxEvent&) { onToolToggle(); });
  m_txtXMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtXMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtYMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtYMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtZMin->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
  m_txtZMax->Bind(wxEVT_TEXT, [this](wxEvent&) { m_hasChanges = true; });
}

void BoundingBoxPanel::onToolToggle()
{
  wxCommandEvent event{m_btnTool->GetValue() ? EToolToggleOn : EToolToggleOff};
  event.SetInt(static_cast<int>(Tool::BoundingBox));

  wxPostEvent(m_panel, event);
}

bool BoundingBoxPanel::hasChanges() const
{
  return m_hasChanges;
}

wxWindow* BoundingBoxPanel::getWxPtr()
{
  return m_panel;
}

void BoundingBoxPanel::populate(const BoundingBox& box)
{
  m_txtXMin->SetValue(std::to_string(box.min[0]));
  m_txtXMax->SetValue(std::to_string(box.max[0]));
  m_txtYMin->SetValue(std::to_string(box.min[1]));
  m_txtYMax->SetValue(std::to_string(box.max[1]));
  m_txtZMin->SetValue(std::to_string(box.min[2]));
  m_txtZMax->SetValue(std::to_string(box.max[2]));

  m_hasChanges = false;
}

class CollisionComponentPanel : public ComponentPanel
{
  public:
    CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore);

    wxPanel* getWxPtr() override;
    bool hasChanges() const override;
    void populate(EntityId entityId) override;
    SystemId systemId() const override;
    void repopulate() override;
    ComponentDataPtr getComponentData() const override;

  private:
    wxPanel* m_panel = nullptr;
    std::unique_ptr<BoundingBoxPanel> m_boundingBoxPanel;
    EntityId m_entityId = NULL_ENTITY_ID;
};

CollisionComponentPanel::CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto boxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Bounding Box");
  m_boundingBoxPanel = std::make_unique<BoundingBoxPanel>(boxSizer->GetStaticBox());
  boxSizer->Add(m_boundingBoxPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(boxSizer, wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);
}

SystemId CollisionComponentPanel::systemId() const
{
  return Systems::Collision;
}

void CollisionComponentPanel::repopulate()
{
  populate(m_entityId);
}

ComponentDataPtr CollisionComponentPanel::getComponentData() const
{
  // TODO: Support other types

  return std::make_unique<ComponentDataWrapper<DStaticBox>>(DStaticBox{
    .restitution = 0.2, // TODO
    .friction = 0.4,    // TODO
    .boundingBox{
      .min{},
      .max{},
      .transform{}
    }
  });
}

bool CollisionComponentPanel::hasChanges() const
{
  // TODO
  return false;
}

void CollisionComponentPanel::populate(EntityId entityId)
{
  m_entityId = entityId;

  // TODO
}

wxPanel* CollisionComponentPanel::getWxPtr()
{
  return m_panel;
}

} // namespace

ComponentPanelPtr createCollisionComponentPanel(wxWindow* parent, EditorCore& editorCore)
{
  return std::make_unique<CollisionComponentPanel>(parent, editorCore);
}

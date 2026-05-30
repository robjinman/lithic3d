#include "entity_edit_mode/component_panel.hpp"
#include "entity_edit_mode/entity_edit_mode.hpp"
#include "transform_panel.hpp"
#include "editor_core.hpp"
#include "tools.hpp"
#include <lithic3d/engine.hpp>
#include <lithic3d/sys_collision.hpp>

using namespace lithic3d;

namespace
{

class BoundingBoxPanel
{
  public:
    explicit BoundingBoxPanel(wxWindow* parent, EntityEditMode& mode);

    wxWindow* getWxPtr();
    bool hasChanges() const;
    void setBoundingBox(const BoundingBox& box);

  private:
    EntityEditMode& m_mode;
    wxPanel* m_panel = nullptr;
    wxButton* m_btnTool = nullptr;
    wxCheckBox* m_chkRender = nullptr;
    TransformPanelPtr m_transformPanel;
    wxTextCtrl* m_txtXMin = nullptr;
    wxTextCtrl* m_txtXMax = nullptr;
    wxTextCtrl* m_txtYMin = nullptr;
    wxTextCtrl* m_txtYMax = nullptr;
    wxTextCtrl* m_txtZMin = nullptr;
    wxTextCtrl* m_txtZMax = nullptr;
    bool m_hasChanges = false;

    void onToolToggle();
    void onRenderToggle();
    void onChange();
    BoundingBox getBoundingBox() const;
};

BoundingBoxPanel::BoundingBoxPanel(wxWindow* parent, EntityEditMode& mode)
  : m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_chkRender = new wxCheckBox(m_panel, wxID_ANY, "Render");
  vbox->Add(m_chkRender, wxSizerFlags().Expand());

  auto transformBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Transform");
  m_btnTool = new wxButton(transformBoxSizer->GetStaticBox(), wxID_ANY, "Edit");
  m_transformPanel = createTransformPanel(transformBoxSizer->GetStaticBox());
  transformBoxSizer->Add(m_btnTool);
  transformBoxSizer->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(transformBoxSizer, wxSizerFlags(1).Expand());

  auto grid = new wxFlexGridSizer(4);

  auto lblXMin = new wxStaticText(m_panel, wxID_ANY, "Min X");
  m_txtXMin = new wxTextCtrl(m_panel, wxID_ANY, "0.0");
  auto lblXMax = new wxStaticText(m_panel, wxID_ANY, "Max X");
  m_txtXMax = new wxTextCtrl(m_panel, wxID_ANY, "0.0");

  auto lblYMin = new wxStaticText(m_panel, wxID_ANY, "Min Y");
  m_txtYMin = new wxTextCtrl(m_panel, wxID_ANY, "0.0");
  auto lblYMax = new wxStaticText(m_panel, wxID_ANY, "Max Y");
  m_txtYMax = new wxTextCtrl(m_panel, wxID_ANY, "0.0");

  auto lblZMin = new wxStaticText(m_panel, wxID_ANY, "Min Z");
  m_txtZMin = new wxTextCtrl(m_panel, wxID_ANY, "0.0");
  auto lblZMax = new wxStaticText(m_panel, wxID_ANY, "Max Z");
  m_txtZMax = new wxTextCtrl(m_panel, wxID_ANY, "0.0");

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

  m_chkRender->Bind(wxEVT_CHECKBOX, [this](wxEvent&) { onRenderToggle(); });

  m_btnTool->Bind(wxEVT_BUTTON, [this](wxEvent&) { onToolToggle(); });

  m_txtXMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_txtXMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_txtYMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_txtYMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_txtZMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_txtZMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
}

void BoundingBoxPanel::onRenderToggle()
{
  m_mode.renderBoundingBox(m_chkRender->GetValue());
}

void BoundingBoxPanel::onChange()
{
  m_hasChanges = true;

  //wxCommandEvent event{EComponentChange};
  //event.SetInt(Systems::Collision);

  //wxPostEvent(m_panel, event);

  m_mode.updateBoundingBox(getBoundingBox());
}

void BoundingBoxPanel::onToolToggle()
{
  //wxCommandEvent event{EToolToggleOn};
  //event.SetInt(static_cast<int>(Tool::BoundingBox));

  //wxPostEvent(m_panel, event);

  m_mode.selectBoundingBox();
}

bool BoundingBoxPanel::hasChanges() const
{
  return m_hasChanges;
}

wxWindow* BoundingBoxPanel::getWxPtr()
{
  return m_panel;
}

void BoundingBoxPanel::setBoundingBox(const BoundingBox& box)
{
  m_transformPanel->setTransform(box.transform);

  m_txtXMin->SetValue(std::to_string(worldUnitsToMetres(box.min[0])));
  m_txtXMax->SetValue(std::to_string(worldUnitsToMetres(box.max[0])));
  m_txtYMin->SetValue(std::to_string(worldUnitsToMetres(box.min[1])));
  m_txtYMax->SetValue(std::to_string(worldUnitsToMetres(box.max[1])));
  m_txtZMin->SetValue(std::to_string(worldUnitsToMetres(box.min[2])));
  m_txtZMax->SetValue(std::to_string(worldUnitsToMetres(box.max[2])));

  m_hasChanges = false;
}

BoundingBox BoundingBoxPanel::getBoundingBox() const
{
  return {
    .min = metresToWorldUnits(Vec3f{
      std::stof(m_txtXMin->GetValue().ToStdString()),
      std::stof(m_txtYMin->GetValue().ToStdString()),
      std::stof(m_txtZMin->GetValue().ToStdString())
    }),
    .max = metresToWorldUnits(Vec3f{
      std::stof(m_txtXMax->GetValue().ToStdString()),
      std::stof(m_txtYMax->GetValue().ToStdString()),
      std::stof(m_txtZMax->GetValue().ToStdString())
    }),
    .transform = m_transformPanel->getTransform()
  };
}

class CollisionComponentPanel : public ComponentPanel
{
  public:
    CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore, EntityEditMode& mode);

    wxPanel* getWxPtr() override;
    bool hasChanges() const override;
    void populate(EntityId entityId) override;
    SystemId systemId() const override;
    //void repopulate() override;
    //ComponentDataPtr getComponentData() const override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    wxPanel* m_panel = nullptr;
    std::unique_ptr<BoundingBoxPanel> m_boundingBoxPanel;
    EntityId m_entityId = NULL_ENTITY_ID;
};

CollisionComponentPanel::CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  auto boxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Bounding Box");
  m_boundingBoxPanel = std::make_unique<BoundingBoxPanel>(boxSizer->GetStaticBox(), m_mode);
  boxSizer->Add(m_boundingBoxPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(boxSizer, wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);
}

SystemId CollisionComponentPanel::systemId() const
{
  return Systems::Collision;
}
/*
void CollisionComponentPanel::repopulate()
{
  populate(m_entityId);
}*/
/*
ComponentDataPtr CollisionComponentPanel::getComponentData() const
{
  // TODO: Support other types

  return std::make_unique<ComponentDataWrapper<DStaticBox>>(DStaticBox{
    .restitution = 0.2, // TODO
    .friction = 0.4,    // TODO
    .boundingBox = m_boundingBoxPanel->getBoundingBox()
  });
}*/

bool CollisionComponentPanel::hasChanges() const
{
  return m_boundingBoxPanel->hasChanges();
}

void CollisionComponentPanel::populate(EntityId entityId)
{
  m_entityId = entityId;

  auto& componentStore = m_core.engine().ecs().componentStore();

  if (componentStore.hasComponentForEntity<CCollisionBox>(entityId)) {
    auto& bboxComp = componentStore.component<CCollisionBox>(entityId);
    m_boundingBoxPanel->setBoundingBox(bboxComp.boundingBox);
  }
  else {
    // TODO
  }
}

wxPanel* CollisionComponentPanel::getWxPtr()
{
  return m_panel;
}

} // namespace

ComponentPanelPtr createCollisionComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
{
  return std::make_unique<CollisionComponentPanel>(parent, editorCore, mode);
}

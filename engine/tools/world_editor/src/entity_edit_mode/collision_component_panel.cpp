#include "entity_edit_mode/component_panel.hpp"
#include "entity_edit_mode/entity_edit_mode.hpp"
#include "transform_panel.hpp"
#include "editor_core.hpp"
#include "tools.hpp"
#include <lithic3d/engine.hpp>
#include <lithic3d/sys_collision.hpp>
#include <wx/spinctrl.h>
#include <wx/choicebk.h>

using namespace lithic3d;

namespace
{

class BoundingBoxPanel
{
  public:
    BoundingBoxPanel(wxWindow* parent, uint32_t index, EntityEditMode& mode);

    wxWindow* getWxPtr();
    bool hasChanges() const;
    void setBoundingBox(const BoundingBox& box, bool resetDirtyFlag = true);
    void setActive();

  private:
    EntityEditMode& m_mode;
    uint32_t m_index; // TODO: Remove
    wxPanel* m_panel = nullptr;
    wxButton* m_btnEdit = nullptr;
    wxCheckBox* m_chkRender = nullptr;
    TransformPanelPtr m_transformPanel;
    wxSpinCtrlDouble* m_spnXMin = nullptr;
    wxSpinCtrlDouble* m_spnXMax = nullptr;
    wxSpinCtrlDouble* m_spnYMin = nullptr;
    wxSpinCtrlDouble* m_spnYMax = nullptr;
    wxSpinCtrlDouble* m_spnZMin = nullptr;
    wxSpinCtrlDouble* m_spnZMax = nullptr;
    bool m_hasChanges = false;

    void onToolToggle();
    void onRenderToggle();
    void onChange();
    BoundingBox getBoundingBox() const;
};

BoundingBoxPanel::BoundingBoxPanel(wxWindow* parent, uint32_t index, EntityEditMode& mode)
  : m_mode(mode)
  , m_index(index)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_chkRender = new wxCheckBox(m_panel, wxID_ANY, "Render");
  vbox->Add(m_chkRender, wxSizerFlags().Expand());

  auto transformBoxSizer = new wxStaticBoxSizer(wxVERTICAL, m_panel, "Transform");
  m_btnEdit = new wxButton(transformBoxSizer->GetStaticBox(), wxID_ANY, "Edit");
  m_transformPanel = createTransformPanel(transformBoxSizer->GetStaticBox());
  transformBoxSizer->Add(m_btnEdit);
  transformBoxSizer->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(transformBoxSizer, wxSizerFlags(1).Expand());

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

  m_btnEdit->Bind(wxEVT_BUTTON, [this](wxEvent&) { onToolToggle(); });

  m_spnXMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnXMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnYMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnYMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnZMin->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
  m_spnZMax->Bind(wxEVT_TEXT, [this](wxEvent&) { onChange(); });
}

void BoundingBoxPanel::setActive()
{
  m_mode.updateBoundingBox(getBoundingBox(), m_index);
}

void BoundingBoxPanel::onRenderToggle()
{
  m_mode.renderBoundingBox(m_index, m_chkRender->GetValue());
}

void BoundingBoxPanel::onChange()
{
  m_hasChanges = true;

  //wxCommandEvent event{EComponentChange};
  //event.SetInt(Systems::Collision);

  //wxPostEvent(m_panel, event);

  m_mode.updateBoundingBox(getBoundingBox(), m_index);
}

void BoundingBoxPanel::onToolToggle()
{
  //wxCommandEvent event{EToolToggleOn};
  //event.SetInt(static_cast<int>(Tool::BoundingBox));

  //wxPostEvent(m_panel, event);

  m_mode.selectBoundingBox(m_index);
}

bool BoundingBoxPanel::hasChanges() const
{
  return m_hasChanges;
}

wxWindow* BoundingBoxPanel::getWxPtr()
{
  return m_panel;
}

void BoundingBoxPanel::setBoundingBox(const BoundingBox& box, bool resetDirtyFlag)
{
  m_transformPanel->setTransform(box.transform);

  m_spnXMin->SetValue(std::to_string(worldUnitsToMetres(box.min[0])));
  m_spnXMax->SetValue(std::to_string(worldUnitsToMetres(box.max[0])));
  m_spnYMin->SetValue(std::to_string(worldUnitsToMetres(box.min[1])));
  m_spnYMax->SetValue(std::to_string(worldUnitsToMetres(box.max[1])));
  m_spnZMin->SetValue(std::to_string(worldUnitsToMetres(box.min[2])));
  m_spnZMax->SetValue(std::to_string(worldUnitsToMetres(box.max[2])));

  m_mode.updateBoundingBox(getBoundingBox(), m_index);

  m_hasChanges = !resetDirtyFlag;
}

BoundingBox BoundingBoxPanel::getBoundingBox() const
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
    }),
    .transform = m_transformPanel->getTransform()
  };
}

class CollisionSubtypePanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;
    virtual void setActive() = 0;
    virtual bool hasChanges() const = 0;
    virtual void repopulateFromMode() = 0;

    virtual ~CollisionSubtypePanel() = default;
};

using CollisionSubtypePanelPtr = std::unique_ptr<CollisionSubtypePanel>;

class DynamicBoxPanel : public CollisionSubtypePanel
{
  public:
    DynamicBoxPanel(wxWindow* parent, EntityId entityId, EditorCore& editorCore,
      EntityEditMode& mode);

    wxWindow* getWxPtr() override;
    void setActive() override;
    bool hasChanges() const override;
    void repopulateFromMode() override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    wxWindow* m_window = nullptr;
    std::unique_ptr<BoundingBoxPanel> m_boundingBoxPanel;
};

DynamicBoxPanel::DynamicBoxPanel(wxWindow* parent, EntityId entityId, EditorCore& editorCore,
  EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
{
  m_window = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  // TODO:
  // - Friction
  // - Restitution
  // - Centre of mass

  auto boxSizer = new wxStaticBoxSizer(wxVERTICAL, m_window, "Bounding Box");
  m_boundingBoxPanel = std::make_unique<BoundingBoxPanel>(boxSizer->GetStaticBox(), 0, mode);
  boxSizer->Add(m_boundingBoxPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(boxSizer, wxSizerFlags(1).Expand());

  m_window->SetSizer(vbox);

  auto& componentStore = m_core.engine().ecs().componentStore();
  assert(componentStore.hasComponentForEntity<CCollisionBox>(entityId));

  auto& bboxComp = componentStore.component<CCollisionBox>(entityId);
  m_boundingBoxPanel->setBoundingBox(bboxComp.boundingBox);
}

void DynamicBoxPanel::setActive()
{
  m_boundingBoxPanel->setActive();
}

bool DynamicBoxPanel::hasChanges() const
{
  return m_boundingBoxPanel->hasChanges();
}

void DynamicBoxPanel::repopulateFromMode()
{
  m_boundingBoxPanel->setBoundingBox(m_mode.getBoundingBox(0), false);
}

wxWindow* DynamicBoxPanel::getWxPtr()
{
  return m_window;
}

class StaticBoxPanel : public CollisionSubtypePanel
{
  public:
    StaticBoxPanel(wxWindow* parent, EntityId entityId, uint32_t index, EditorCore& editorCore,
      EntityEditMode& mode);

    wxWindow* getWxPtr() override;
    void setActive() override;
    bool hasChanges() const override;
    void repopulateFromMode() override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    uint32_t m_index;
    wxWindow* m_window = nullptr;
    std::unique_ptr<BoundingBoxPanel> m_boundingBoxPanel;
};

StaticBoxPanel::StaticBoxPanel(wxWindow* parent, EntityId entityId, uint32_t index,
  EditorCore& editorCore, EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
  , m_index(index)
{
  m_window = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  // TODO:
  // - Friction
  // - Restitution

  auto boxSizer = new wxStaticBoxSizer(wxVERTICAL, m_window, "Bounding Box");
  m_boundingBoxPanel = std::make_unique<BoundingBoxPanel>(boxSizer->GetStaticBox(), m_index, mode);
  boxSizer->Add(m_boundingBoxPanel->getWxPtr(), wxSizerFlags(1).Expand());

  vbox->Add(boxSizer, wxSizerFlags(1).Expand());

  m_window->SetSizer(vbox);

  auto& componentStore = m_core.engine().ecs().componentStore();
  assert(componentStore.hasComponentForEntity<CCollisionBox>(entityId));

  auto& bboxComp = componentStore.component<CCollisionBox>(entityId);
  m_boundingBoxPanel->setBoundingBox(bboxComp.boundingBox);
}

void StaticBoxPanel::setActive()
{
  m_boundingBoxPanel->setActive();
}

bool StaticBoxPanel::hasChanges() const
{
  return m_boundingBoxPanel->hasChanges();
}

void StaticBoxPanel::repopulateFromMode()
{
  m_boundingBoxPanel->setBoundingBox(m_mode.getBoundingBox(m_index), false);
}

wxWindow* StaticBoxPanel::getWxPtr()
{
  return m_window;
}

class AggregatePanel : public CollisionSubtypePanel
{
  public:
    AggregatePanel(wxWindow* parent, EntityId entityId, EditorCore& editorCore,
      EntityEditMode& mode);

    wxWindow* getWxPtr() override;
    void setActive() override;
    bool hasChanges() const override;
    void repopulateFromMode() override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    EntityId m_entityId;
    wxWindow* m_window = nullptr;
    wxChoice* m_cboType = nullptr;
    wxPanel* m_partInfoPanel = nullptr;
    std::vector<CollisionSubtypePanelPtr> m_partPanels;
    wxChoicebook* m_chcParts = nullptr;

    void onPartCreate();
    void onPartSelect();
};

template<typename T>
class ClientData : public wxClientData
{
  public:
    ClientData(const T& value)
      : value(value) {}

    ClientData(T&& value)
      : value(std::move(value)) {}

    T value;
};

AggregatePanel::AggregatePanel(wxWindow* parent, EntityId entityId, EditorCore& editorCore,
  EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
  , m_entityId(entityId)
{
  m_window = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_chcParts = new wxChoicebook(m_window, wxID_ANY);

  m_cboType = new wxChoice(m_window, wxID_ANY);
  m_cboType->Insert("Static box", 0, new ClientData(CollisionComponentType::StaticBox));
  m_cboType->Insert("Polyhedron", 1, new ClientData(CollisionComponentType::Polyhedron));

  auto btnCreate = new wxButton(m_window, wxID_ANY, "Create new");
  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(m_cboType, wxSizerFlags(1).Expand());
  hbox->Add(btnCreate, wxSizerFlags(1).Expand());

  m_partInfoPanel = new wxPanel(m_window, wxID_ANY);

  vbox->Add(m_chcParts, wxSizerFlags(1).Expand());
  vbox->Add(hbox, wxSizerFlags(0).Expand());
  vbox->Add(m_partInfoPanel, wxSizerFlags(1).Expand());

  m_window->SetSizer(vbox);

  btnCreate->Bind(wxEVT_BUTTON, [this](wxEvent&) { onPartCreate(); });
  m_chcParts->Bind(wxEVT_CHOICEBOOK_PAGE_CHANGED, [this](wxEvent&) { onPartSelect(); });

  auto& sysCollision = m_core.engine().ecs().system<SysCollision>();
  auto& children = sysCollision.getAggregateChildren(m_entityId);

  m_chcParts->DeleteAllPages();
  m_partPanels.clear();

  for (size_t i = 0; i < children.size(); ++i) {
    auto type = sysCollision.componentType(children[i]);
    std::string typeName = "";
    CollisionSubtypePanelPtr panel;

    switch (type) {
      case CollisionComponentType::StaticBox: {
        typeName = "Static Box";
        panel = std::make_unique<StaticBoxPanel>(m_window, children[i], i, m_core, m_mode);
        break;
      }
      default: {
        EXCEPTION("Unexpected component type in aggregate");
      }
    }

    m_partPanels.push_back(std::move(panel));
    m_chcParts->AddPage(m_partPanels.back()->getWxPtr(), STR("[" << i << "] " << typeName));
  }

  m_chcParts->SetSelection(0);
}

bool AggregatePanel::hasChanges() const
{
  for (auto& panel : m_partPanels) {
    if (panel->hasChanges()) {
      return true;
    }
  }

  return false;
}

void AggregatePanel::repopulateFromMode()
{
  for (auto& panel : m_partPanels) {
    panel->repopulateFromMode();
  }
}

wxWindow* AggregatePanel::getWxPtr()
{
  return m_window;
}

void AggregatePanel::setActive()
{
  onPartSelect();
}

void AggregatePanel::onPartSelect()
{
  int idx = m_chcParts->GetSelection();
  if (idx == wxNOT_FOUND) {
    return;
  }

  m_partPanels[idx]->setActive();
}

void AggregatePanel::onPartCreate()
{
  auto& componentStore = m_core.engine().ecs().componentStore();
  auto& sysCollision = m_core.engine().ecs().system<SysCollision>();

  auto clientData = m_cboType->GetClientObject(m_cboType->GetSelection());

  if (clientData == nullptr) {
    return;
  }

  auto type = dynamic_cast<const ClientData<CollisionComponentType>*>(clientData)->value;

  size_t n = sysCollision.getAggregateChildren(m_entityId).size();

  CollisionSubtypePanelPtr panel;
  std::string typeName = "";

  switch (type) {
    case CollisionComponentType::StaticBox: {
      typeName = "Static box";

      auto partId = sysCollision.addPartToAggregate(m_entityId, CollisionComponentType::StaticBox);
      m_mode.addBoundingBox(componentStore.component<CCollisionBox>(partId).boundingBox);
      panel = std::make_unique<StaticBoxPanel>(m_window, partId, n, m_core, m_mode);

      break;
    }
    default: {
      EXCEPTION("Unexpected component type in aggregate");
    }
  }

  m_partPanels.push_back(std::move(panel));
  m_chcParts->AddPage(m_partPanels.back()->getWxPtr(), STR("[" << n << "] " << typeName));

  m_chcParts->SetSelection(n);
}

class CollisionComponentPanel : public ComponentPanel
{
  public:
    CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore, EntityEditMode& mode);

    wxWindow* getWxPtr() override;
    bool hasChanges() const override;
    void populate(EntityId entityId) override;
    void repopulateFromMode() override;

  private:
    EditorCore& m_core;
    EntityEditMode& m_mode;
    CollisionSubtypePanelPtr m_componentPanel = nullptr;
    wxPanel* m_panel = nullptr;
};

CollisionComponentPanel::CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);
}

bool CollisionComponentPanel::hasChanges() const
{
  if (m_componentPanel != nullptr) {
    return m_componentPanel->hasChanges();
  }
  return false;
}

void CollisionComponentPanel::populate(EntityId entityId)
{
  m_panel->DestroyChildren();
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_panel->SetSizer(vbox, true);

  auto& sysCollision = m_core.engine().ecs().system<SysCollision>();

  wxString typeName = "";
  auto type = sysCollision.componentType(entityId);

  switch (type) {
    case CollisionComponentType::StaticBox: {
      typeName = "Static Box";
      m_componentPanel = std::make_unique<StaticBoxPanel>(m_panel, entityId, 0, m_core, m_mode);
      break;
    }
    case CollisionComponentType::DynamicBox: {
      typeName = "Dynamic Box";
      m_componentPanel = std::make_unique<DynamicBoxPanel>(m_panel, entityId, m_core, m_mode);
      break;
    }
    case CollisionComponentType::Aggregate: {
      typeName = "Aggregate";
      m_componentPanel = std::make_unique<AggregatePanel>(m_panel, entityId, m_core, m_mode);
      break;
    }
    default: EXCEPTION("Unrecognised collision component type");
  }

  if (m_componentPanel != nullptr) {
    auto lblComponentType = new wxStaticText(m_panel, wxID_ANY, STR("Type: " << typeName));

    vbox->Add(lblComponentType, wxSizerFlags(0).Expand());
    vbox->Add(m_componentPanel->getWxPtr(), wxSizerFlags(1).Expand());
  }

  m_panel->Layout();
}

void CollisionComponentPanel::repopulateFromMode()
{
  m_componentPanel->repopulateFromMode();
}

wxWindow* CollisionComponentPanel::getWxPtr()
{
  return m_panel;
}

} // namespace

ComponentPanelPtr createCollisionComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
{
  return std::make_unique<CollisionComponentPanel>(parent, editorCore, mode);
}

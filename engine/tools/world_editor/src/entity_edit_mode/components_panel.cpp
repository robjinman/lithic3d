#include "entity_edit_mode/components_panel.hpp"
#include "entity_edit_mode/component_panel.hpp"
#include "entity_edit_mode/entity_edit_mode.hpp"
#include "editor_core.hpp"
#include <lithic3d/engine.hpp>
#include <wx/wx.h>
#include <wx/choicebk.h>

using namespace lithic3d;

wxDEFINE_EVENT(EComponentChange, wxCommandEvent);

ComponentPanelPtr createSpatialComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode);
ComponentPanelPtr createCollisionComponentPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode);

namespace
{

class ComponentsPanelImpl : public ComponentsPanel
{
  public:
    ComponentsPanelImpl(wxWindow* parent, EditorCore& editorCore, EntityEditMode& mode);

    void onEntitySelect(EntityId entityId) override;
    //void repopulate() override;
    SystemId getSelectedSystem() const override;
    //ComponentDataPtr getComponentData() const override;

    wxWindow* getWxPtr() override;

  private:
    wxPanel* m_panel = nullptr;
    wxChoicebook* m_choicebook = nullptr;
    EditorCore& m_core;
    EntityEditMode& m_mode;
    std::vector<ComponentPanelPtr> m_panels;
    EntityId m_entityId = NULL_ENTITY_ID;

    ComponentPanelPtr createComponentPanel(SystemId systemId);
    void onResetClick();
    void onApplyClick();
    bool hasChanges() const;
};

ComponentsPanelImpl::ComponentsPanelImpl(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
  : m_core(editorCore)
  , m_mode(mode)
{
  m_panel = new wxPanel(parent, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_choicebook = new wxChoicebook(m_panel, wxID_ANY);

  wxButton* btnReset = new wxButton(m_panel, wxID_ANY, "Reset");
  wxButton* btnApply = new wxButton(m_panel, wxID_ANY, "Apply");

  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(btnReset, wxSizerFlags(1).Expand());
  hbox->Add(btnApply, wxSizerFlags(1).Expand());

  vbox->Add(m_choicebook, wxSizerFlags(1).Expand());
  vbox->Add(hbox, wxSizerFlags().Expand());

  m_panel->SetSizer(vbox);
  m_panel->Layout();

  btnReset->Bind(wxEVT_BUTTON, [this](wxEvent&) { onResetClick(); });
  btnReset->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(hasChanges()); });
  btnApply->Bind(wxEVT_BUTTON, [this](wxEvent&) { onApplyClick(); });
  btnApply->Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Enable(hasChanges()); });
}

SystemId ComponentsPanelImpl::getSelectedSystem() const
{
  int pageIndex = m_choicebook->GetSelection();
  if (pageIndex >= 0) {
    auto& panel = m_panels.at(pageIndex);
    return panel->systemId();
  }
  return std::numeric_limits<SystemId>::max();
}
/*
ComponentDataPtr ComponentsPanelImpl::getComponentData() const
{
  int pageIndex = m_choicebook->GetSelection();
  if (pageIndex >= 0) {
    auto& panel = m_panels.at(pageIndex);
    return panel->getComponentData();
  }
  return nullptr;
}*/
/*
void ComponentsPanelImpl::repopulate()
{
  int pageIndex = m_choicebook->GetSelection();
  if (pageIndex >= 0) {
    auto& panel = m_panels.at(pageIndex);
    panel->repopulate();
  }
}*/

bool ComponentsPanelImpl::hasChanges() const
{
  int pageIndex = m_choicebook->GetSelection();

  if (pageIndex < 0) {
    return false;
  }

  auto& panel = m_panels.at(pageIndex);
  return panel->hasChanges();
}

void ComponentsPanelImpl::onResetClick()
{
  for (size_t i = 0; i < m_panels.size(); ++i) {
    m_panels[i]->populate(m_entityId);
  }
}

void ComponentsPanelImpl::onApplyClick()
{
  // TODO: Show warning if entities affected

  // TODO
}

wxWindow* ComponentsPanelImpl::getWxPtr()
{
  return m_panel;
}

ComponentPanelPtr ComponentsPanelImpl::createComponentPanel(SystemId systemId)
{
  switch (systemId) {
    case Systems::Spatial: return createSpatialComponentPanel(m_choicebook, m_core, m_mode);
    case Systems::Collision: return createCollisionComponentPanel(m_choicebook, m_core, m_mode);
    // ...
    default: return nullptr;
  }
}

void ComponentsPanelImpl::onEntitySelect(EntityId entityId)
{
  m_choicebook->DeleteAllPages();
  m_panels.clear();

  auto& ecs = m_core.engine().ecs();

  m_entityId = entityId;

  for (SystemId systemId = 0; systemId < ecs.numSystems(); ++systemId) {
    auto& system = ecs.getSystem(systemId);
    auto panel = createComponentPanel(systemId);
    if (panel != nullptr) {
      panel->populate(m_entityId);
      m_choicebook->AddPage(panel->getWxPtr(), system.name().c_str());
      m_panels.push_back(std::move(panel));
    }
  }

  m_choicebook->Layout();
}

} // namespace

ComponentsPanelPtr createComponentsPanel(wxWindow* parent, EditorCore& editorCore,
  EntityEditMode& mode)
{
  return std::make_unique<ComponentsPanelImpl>(parent, editorCore, mode);
}

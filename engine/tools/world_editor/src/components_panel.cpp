#include "components_panel.hpp"
#include "component_panel.hpp"
#include "editor_core.hpp"
#include <lithic3d/engine.hpp>
#include <lithic3d/ecs.hpp>
#include <lithic3d/systems.hpp>
#include <wx/wx.h>
#include <wx/notebook.h>

using namespace lithic3d;

ComponentPanelPtr createSpatialComponentPanel(wxWindow* parent, EditorCore& editorCore);
//ComponentPanelPtr createCollisionComponentPanel(wxWindow* parent, EditorCore& editorCore);

namespace
{

class ComponentsPanelImpl : public ComponentsPanel
{
  public:
    ComponentsPanelImpl(wxWindow* parent, EditorCore& editorCore);

    void onEntitySelect(EntityId entityId) override;
    void onPrefabSelect(const std::string& prefab) override;

    wxWindow* getWxPtr() override;

  private:
    wxPanel* m_panel = nullptr;
    wxNotebook* m_notebook = nullptr;
    EditorCore& m_core;
    std::map<EntityId, ComponentPanelPtr> m_panels;

    ComponentPanelPtr createComponentPanel(SystemId systemId);
    void onCancelClick();
    void onApplyClick();
};

ComponentsPanelImpl::ComponentsPanelImpl(wxWindow* parent, EditorCore& editorCore)
  : m_core(editorCore)
{
  m_panel = new wxPanel(parent, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_panel->SetSizer(vbox);

  m_notebook = new wxNotebook(m_panel, wxID_ANY);

  wxButton* btnCancel = new wxButton(m_panel, wxID_ANY, "Cancel");
  wxButton* btnApply = new wxButton(m_panel, wxID_ANY, "Apply");

  auto hbox = new wxBoxSizer(wxHORIZONTAL);
  hbox->Add(btnCancel);
  hbox->Add(btnApply);

  vbox->Add(m_notebook, 1, wxEXPAND);
  vbox->Add(hbox);

  btnCancel->Bind(wxEVT_BUTTON, [this](wxEvent&) { onCancelClick(); });
  btnApply->Bind(wxEVT_BUTTON, [this](wxEvent&) { onApplyClick(); });
}

void ComponentsPanelImpl::onCancelClick()
{
  // TODO
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
    case Systems::Spatial: return createSpatialComponentPanel(m_notebook, m_core);
    //case Systems::Collision: return createCollisionComponentPanel(m_notebook, m_core);
    // ...
    default: return nullptr;
  }
}

void ComponentsPanelImpl::onPrefabSelect(const std::string& prefab)
{
  // TODO
}

void ComponentsPanelImpl::onEntitySelect(EntityId entityId)
{
  while (m_notebook->GetPageCount() > 0) {
    m_notebook->RemovePage(0);
  }

  auto& ecs = m_core.engine().ecs();

  for (SystemId systemId = 0; systemId < ecs.numSystems(); ++systemId) {
    auto& system = ecs.getSystem(systemId);
    auto panel = createComponentPanel(systemId);
    if (panel != nullptr) {
      panel->populate(entityId);
      m_notebook->AddPage(panel->getWxPtr(), system.name().c_str());
      m_panels.insert({ systemId, std::move(panel) });
    }
  }

  m_notebook->Layout();
}

} // namespace

ComponentsPanelPtr createComponentsPanel(wxWindow* parent, EditorCore& editorCore)
{
  return std::make_unique<ComponentsPanelImpl>(parent, editorCore);
}

#include "component_panel.hpp"
#include "transform_panel.hpp"
#include "editor_core.hpp"
#include <wx/wx.h>

using namespace lithic3d;

namespace
{

class CollisionComponentPanel : public ComponentPanel
{
  public:
    CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore);

    wxPanel* getWxPtr() override;
    void populate(EntityId entityId) override;

  private:
    wxPanel* m_panel = nullptr;
    TransformPanelPtr m_transformPanel = nullptr;

    void onEntitySelect();
};

CollisionComponentPanel::CollisionComponentPanel(wxWindow* parent, EditorCore& editorCore)
{
  m_panel = new wxPanel(parent, wxID_ANY);

  auto vbox = new wxBoxSizer(wxVERTICAL);

  m_transformPanel = createTransformPanel(m_panel);
  vbox->Add(m_transformPanel->getWxPtr(), wxSizerFlags(1).Expand());

  m_panel->SetSizer(vbox);
}

void CollisionComponentPanel::populate(EntityId entityId)
{
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

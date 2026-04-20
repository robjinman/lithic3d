#include "prefabs_panel.hpp"
#include <lithic3d/utils.hpp>
#include <lithic3d/xml.hpp>

namespace fs = std::filesystem;

namespace
{

class PrefabsPanelImpl : public PrefabsPanel
{
  public:
    PrefabsPanelImpl(wxWindow* parent, const fs::path& projectRoot);

    void populate() override;
    wxPanel* getWxPtr() override;

  private:
    fs::path m_projectRoot;
    wxPanel* m_panel;
    wxListBox* m_listBox;
};

PrefabsPanelImpl::PrefabsPanelImpl(wxWindow* parent, const fs::path& projectRoot)
  : m_projectRoot(projectRoot)
{
  m_panel = new wxPanel(parent);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_panel->SetSizer(vbox);
  m_listBox = new wxListBox{m_panel, wxID_ANY};
  m_panel->GetSizer()->Add(m_listBox, 1, wxEXPAND | wxALL);
}

wxPanel* PrefabsPanelImpl::getWxPtr()
{
  return m_panel;
}

void PrefabsPanelImpl::populate()
{
  fs::directory_iterator it{m_projectRoot / "data/prefabs"};

  size_t i = 0;
  auto end = fs::end(it);
  for (; it != end; ++it) {
    if (it->is_regular_file()) {
      auto xmlPrefab = lithic3d::parseXml(lithic3d::readBinaryFile(it->path()));
      m_listBox->Insert(xmlPrefab->attribute("name"), i++);
    }
  }
}

} // namespace

PrefabsPanelPtr createPrefabsPanel(wxWindow* parent, const fs::path& projectRoot)
{
  return std::make_unique<PrefabsPanelImpl>(parent, projectRoot);
}

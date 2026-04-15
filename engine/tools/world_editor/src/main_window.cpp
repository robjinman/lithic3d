#include <sstream>
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <lithic3d/lithic3d.hpp>
#include "canvas.hpp"

namespace fs = std::filesystem;

using namespace lithic3d;

namespace
{

enum MenuItems
{
  MNU_OPEN = wxID_HIGHEST + 1
};

class MainWindow : public wxFrame
{
  public:
    explicit MainWindow(const wxString& title);

    ~MainWindow() override;

  private:
    void constructMenu();
    void constructLeftPanel();
    void constructRightPanel();
    void constructPrefabsPanel();
    void populatePrefabsPanel();
    EntityId constructLight();

    void onOpen(wxCommandEvent& e);
    void onExit(wxCommandEvent& e);
    void onAbout(wxCommandEvent& e);
    void onClose(wxCloseEvent& e);

    wxSplitterWindow* m_splitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxNotebook* m_rightPanel = nullptr;
    wxPanel* m_prefabsPanel = nullptr;
    Canvas* m_canvas = nullptr;
    fs::path m_projectRoot;
};

MainWindow::MainWindow(const wxString& title)
  : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition)
{
  Bind(wxEVT_MENU, &MainWindow::onOpen, this, MNU_OPEN);
  Bind(wxEVT_MENU, &MainWindow::onExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &MainWindow::onAbout, this, wxID_ABOUT);
  Bind(wxEVT_CLOSE_WINDOW, &MainWindow::onClose, this);

  constructMenu();

  m_vbox = new wxBoxSizer(wxVERTICAL);
  SetSizer(m_vbox);

  SetAutoLayout(true);

  m_splitter = new wxSplitterWindow(this);
  m_splitter->SetMinimumPaneSize(300);

  m_vbox->Add(m_splitter, 1, wxEXPAND, 0);

  constructLeftPanel();
  constructRightPanel();

  m_splitter->SplitVertically(m_leftPanel, m_rightPanel, 10000);

  CreateStatusBar();
  SetStatusText(wxEmptyString);

  Maximize();
}

void MainWindow::onOpen(wxCommandEvent&)
{
  auto dialog = new wxDirDialog{this, "Open directory"};

  if (dialog->ShowModal() != wxID_OK) {
    return;
  }

  m_projectRoot = dialog->GetPath().ToStdString();

  m_canvas->startEngine(m_projectRoot);
  constructPrefabsPanel();
  populatePrefabsPanel();

  constructLight();
}

void MainWindow::onClose(wxCloseEvent& e)
{
  e.Skip();
}

EntityId MainWindow::constructLight()
{
  auto id = m_canvas->engine().ecs().idGen().getNewEntityId();
  m_canvas->engine().ecs().componentStore().allocate<DSpatial, DDirectionalLight>(id);

  DSpatial spatial{
    .transform = {},
    .parent = m_canvas->engine().ecs().system<SysSpatial>().root(),
    .enabled = true,
    .aabb{}
  };

  m_canvas->engine().ecs().system<SysSpatial>().addEntity(id, spatial);

  auto light = std::make_unique<DDirectionalLight>();
  light->colour = { 1.f, 0.9f, 0.9f };
  light->ambient = 0.4f;
  light->specular = 0.9f;

  m_canvas->engine().ecs().system<SysRender3d>().addEntity(id, std::move(light));

  return id;
}

void MainWindow::constructMenu()
{
  wxMenu* mnuFile = new wxMenu;
  auto itmOpen = new wxMenuItem(mnuFile, MNU_OPEN, "Open");
  mnuFile->Append(itmOpen);
  mnuFile->Append(wxID_EXIT);

  wxMenu* mnuHelp = new wxMenu;
  mnuHelp->Append(wxID_ABOUT);

  wxMenuBar* menuBar = new wxMenuBar;
  menuBar->Append(mnuFile, wxGetTranslation("&File"));
  menuBar->Append(mnuHelp, wxGetTranslation("&Help"));

  SetMenuBar(menuBar);
}

void MainWindow::constructLeftPanel()
{
  assert(m_splitter);

  m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_canvas = createCanvas(m_leftPanel);
  vbox->Add(m_canvas, 1, wxEXPAND | wxALL, 5);
  m_leftPanel->SetSizer(vbox);
  m_leftPanel->SetCanFocus(true);
}

void MainWindow::constructPrefabsPanel()
{
  assert(m_rightPanel);

  m_prefabsPanel = new wxPanel(m_rightPanel);
  m_rightPanel->AddPage(m_prefabsPanel, "Prefabs");
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_prefabsPanel->SetSizer(vbox);
}

void MainWindow::constructRightPanel()
{
  assert(m_splitter);
  m_rightPanel = new wxNotebook(m_splitter, wxID_ANY);
}

void MainWindow::populatePrefabsPanel()
{
  assert(m_prefabsPanel);

  fs::directory_iterator it{m_projectRoot / "data/prefabs"};
  auto listBox = new wxListBox{m_prefabsPanel, wxID_ANY};
  m_prefabsPanel->GetSizer()->Add(listBox, 1, wxEXPAND | wxALL);

  size_t i = 0;
  auto end = fs::end(it);
  for (; it != end; ++it) {
    if (it->is_regular_file()) {
      auto xmlPrefab = parseXml(readBinaryFile(it->path()));
      listBox->Insert(xmlPrefab->attribute("name"), i++);
    }
  }
}

void MainWindow::onExit(wxCommandEvent&)
{
  Close();
}

void MainWindow::onAbout(wxCommandEvent&)
{
  std::stringstream ss;
  ss << "Lithic3D World Editor." << std::endl << std::endl;
  ss << "Copyright Freehold Apps Ltd 2025 - 2026. All rights reserved.";

  wxMessageBox(wxGetTranslation(ss.str()), "Lithic3D World Editor", wxOK | wxICON_INFORMATION);
}

MainWindow::~MainWindow()
{
}

} // namespace

wxFrame* createMainWindow(const wxString& title)
{
  return new MainWindow(title);
}

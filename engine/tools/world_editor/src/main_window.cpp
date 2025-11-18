#include "canvas.hpp"
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <sstream>

namespace
{

class MainWindow : public wxFrame
{
  public:
    MainWindow(const wxString& title, const wxSize& size);

  private:
    void constructMenu();
    void constructLeftPanel();
    void constructRightPanel();

    void onExit(wxCommandEvent& e);
    void onAbout(wxCommandEvent& e);
    void onClose(wxCloseEvent& e);

    wxSplitterWindow* m_splitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxNotebook* m_rightPanel = nullptr;
    Canvas* m_canvas = nullptr;
};

MainWindow::MainWindow(const wxString& title, const wxSize& size)
  : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, size)
{
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

  m_splitter->SplitVertically(m_leftPanel, m_rightPanel, 500); // TODO

  CreateStatusBar();
  SetStatusText(wxEmptyString);
}

void MainWindow::onClose(wxCloseEvent& e)
{
  e.Skip();
}

void MainWindow::constructMenu()
{
  wxMenu* mnuFile = new wxMenu;
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
  m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_leftPanel->SetSizer(vbox);
  m_leftPanel->SetCanFocus(true);

  m_canvas = createCanvas(m_leftPanel);

  vbox->Add(m_canvas, 1, wxEXPAND | wxALL, 5);
}

void MainWindow::constructRightPanel()
{
  m_rightPanel = new wxNotebook(m_splitter, wxID_ANY);
}

void MainWindow::onExit(wxCommandEvent&)
{
  Close();
}

void MainWindow::onAbout(wxCommandEvent&)
{
  std::stringstream ss;
  ss << "Lithic3D World Editor." << std::endl << std::endl;
  ss << "Copyright Freehold Apps Ltd 2025. All rights reserved.";

  wxMessageBox(wxGetTranslation(ss.str()), "Lithic3D World Editor", wxOK | wxICON_INFORMATION);
}

} // namespace

wxFrame* createMainWindow(const wxString& title, const wxSize& size)
{
  return new MainWindow(title, size);
}

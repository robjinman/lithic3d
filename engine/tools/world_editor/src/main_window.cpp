#include "world_editor.hpp"
#include "prefabs_panel.hpp"
#include "transform_panel.hpp"
#include <sstream>
#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/notebook.h>
#include <lithic3d/lithic3d.hpp>

namespace fs = std::filesystem;

#ifdef PLATFORM_OSX
void osxResizeMetalLayer(WXWidget window, int width, int height);
#endif

lithic3d::WindowDelegatePtr createWindowDelegate(WXWidget window);

using namespace lithic3d;

namespace
{

enum MenuItems
{
  MNU_OPEN = wxID_HIGHEST + 1,
  MNU_SAVE
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
    void populateRightPanel();

    void onOpen(wxCommandEvent& e);
    void onSave(wxCommandEvent& e);
    void onExit(wxCommandEvent& e);
    void onAbout(wxCommandEvent& e);
    void onClose(wxCloseEvent& e);
    void onTick(wxTimerEvent& e);

    void onCanvasResize(wxSizeEvent& e);
    void onCanvasKeyDown(wxKeyEvent& e);
    void onCanvasKeyUp(wxKeyEvent& e);
    void onCanvasLeftMouseBtnDown(wxMouseEvent& e);
    void onCanvasLeftMouseBtnUp(wxMouseEvent& e);
    void onCanvasMouseMove(wxMouseEvent& e);

    wxSplitterWindow* m_splitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxPanel* m_rightSidePanel = nullptr;
    wxNotebook* m_rightSidePanelTopWindow = nullptr;
    wxNotebook* m_rightSidePanelBottomWindow = nullptr;
    PrefabsPanelPtr m_prefabsPanel = nullptr;
    TransformPanelPtr m_transformPanel = nullptr;
    wxPanel* m_canvas = nullptr;
    wxTimer* m_timer = nullptr;
    WindowDelegatePtr m_windowDelegate;
    WorldEditorPtr m_worldEditor;
};

MainWindow::MainWindow(const wxString& title)
  : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition)
{
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

  m_splitter->SplitVertically(m_leftPanel, m_rightSidePanel, 10000);

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

  m_windowDelegate = createWindowDelegate(m_canvas->GetHandle());
  m_worldEditor = createWorldEditor(dialog->GetPath().ToStdString(), *m_windowDelegate);

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);

  populateRightPanel();
  m_prefabsPanel->populate();

  m_canvas->Bind(wxEVT_SIZE, &MainWindow::onCanvasResize, this);
  m_canvas->Bind(wxEVT_KEY_DOWN, &MainWindow::onCanvasKeyDown, this);
  m_canvas->Bind(wxEVT_KEY_UP, &MainWindow::onCanvasKeyUp, this);
  m_canvas->Bind(wxEVT_LEFT_DOWN, &MainWindow::onCanvasLeftMouseBtnDown, this);
  m_canvas->Bind(wxEVT_LEFT_UP, &MainWindow::onCanvasLeftMouseBtnUp, this);
  m_canvas->Bind(wxEVT_MOTION, &MainWindow::onCanvasMouseMove, this);

  Bind(wxEVT_TIMER, &MainWindow::onTick, this);
}

void MainWindow::onSave(wxCommandEvent&)
{
  m_worldEditor->saveChanges();
}

void MainWindow::onClose(wxCloseEvent& e)
{
  e.Skip();
}

KeyboardKey mapToLithic3dKey(int code)
{
  if (code >= 'A' && code <= 'Z') {
    return static_cast<KeyboardKey>(code);
  }
  if (code >= '0' && code <= '9') {
    return static_cast<KeyboardKey>(code);
  }
  switch (code) {
    case WXK_SPACE: return KeyboardKey::Space;
    case WXK_ESCAPE: return KeyboardKey::Escape;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER: return KeyboardKey::Enter;
    case WXK_BACK: return KeyboardKey::Backspace;
    case WXK_F1: return KeyboardKey::F1;
    case WXK_F2: return KeyboardKey::F2;
    case WXK_F3: return KeyboardKey::F3;
    case WXK_F4: return KeyboardKey::F4;
    case WXK_F5: return KeyboardKey::F5;
    case WXK_F6: return KeyboardKey::F6;
    case WXK_F7: return KeyboardKey::F7;
    case WXK_F8: return KeyboardKey::F8;
    case WXK_F9: return KeyboardKey::F9;
    case WXK_F10: return KeyboardKey::F10;
    case WXK_F11: return KeyboardKey::F11;
    case WXK_F12: return KeyboardKey::F12;
    case WXK_RIGHT: return KeyboardKey::Right;
    case WXK_LEFT: return KeyboardKey::Left;
    case WXK_DOWN: return KeyboardKey::Down;
    case WXK_UP: return KeyboardKey::Up;
    default: return KeyboardKey::Unknown;
  }
}

void MainWindow::onCanvasKeyDown(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_worldEditor->onKeyDown(key);
}

void MainWindow::onCanvasKeyUp(wxKeyEvent& e)
{
  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_worldEditor->onKeyUp(key);
}

void MainWindow::onCanvasResize(wxSizeEvent& e)
{
  e.Skip();

  m_worldEditor->onCanvasResize(e.GetSize().GetWidth(), e.GetSize().GetHeight());

#ifdef PLATFORM_OSX
  osxResizeMetalLayer(GetHandle(), e.GetSize().GetWidth(), e.GetSize().GetHeight());
#endif
}

void MainWindow::onCanvasLeftMouseBtnDown(wxMouseEvent& e)
{
  e.Skip();

  m_canvas->SetFocus();
  m_worldEditor->onMouseLeftBtnDown();
}

void MainWindow::onCanvasLeftMouseBtnUp(wxMouseEvent& e)
{
  e.Skip();

  m_worldEditor->onMouseLeftBtnUp();
}

void MainWindow::onCanvasMouseMove(wxMouseEvent& e)
{
  e.Skip();

  if (m_worldEditor) {
    float x = static_cast<float>(e.GetX()) / GetClientSize().GetWidth();
    float y = static_cast<float>(e.GetY()) / GetClientSize().GetHeight();
    m_worldEditor->onMouseMove(x, y);
  }
}

void MainWindow::onTick(wxTimerEvent&)
{
  m_worldEditor->update();
}

void MainWindow::constructMenu()
{
  wxMenu* mnuFile = new wxMenu;
  auto itmOpen = new wxMenuItem(mnuFile, MNU_OPEN, "Open");
  mnuFile->Append(itmOpen);
  auto itmSave = new wxMenuItem(mnuFile, MNU_SAVE, "Save");
  mnuFile->Append(itmSave);
  mnuFile->Append(wxID_EXIT);

  wxMenu* mnuHelp = new wxMenu;
  mnuHelp->Append(wxID_ABOUT);

  wxMenuBar* menuBar = new wxMenuBar;
  menuBar->Append(mnuFile, wxGetTranslation("&File"));
  menuBar->Append(mnuHelp, wxGetTranslation("&Help"));

  SetMenuBar(menuBar);

  Bind(wxEVT_MENU, &MainWindow::onOpen, this, MNU_OPEN);
  Bind(wxEVT_MENU, &MainWindow::onSave, this, MNU_SAVE);
  Bind(wxEVT_MENU, &MainWindow::onExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &MainWindow::onAbout, this, wxID_ABOUT);
}

void MainWindow::constructLeftPanel()
{
  assert(m_splitter);

  m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_canvas = new wxPanel(m_leftPanel);
  vbox->Add(m_canvas, 1, wxEXPAND, 5);
  m_leftPanel->SetSizer(vbox);
  m_leftPanel->SetCanFocus(true);
}

void MainWindow::constructRightPanel()
{
  m_rightSidePanel = new wxPanel(m_splitter);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_rightSidePanel->SetSizer(vbox);

  m_rightSidePanelTopWindow = new wxNotebook(m_rightSidePanel, wxID_ANY);
  m_rightSidePanelBottomWindow = new wxNotebook(m_rightSidePanel, wxID_ANY);

  vbox->Add(m_rightSidePanelTopWindow, 1, wxEXPAND);
  vbox->Add(m_rightSidePanelBottomWindow, 1, wxEXPAND);
}

void MainWindow::populateRightPanel()
{
  assert(m_rightSidePanelTopWindow);
  assert(m_rightSidePanelBottomWindow);

  m_prefabsPanel = createPrefabsPanel(m_rightSidePanelTopWindow, *m_worldEditor);
  m_transformPanel = createTransformPanel(m_rightSidePanelBottomWindow, *m_worldEditor);

  m_rightSidePanelTopWindow->AddPage(m_prefabsPanel->getWxPtr(), "Prefabs");
  m_rightSidePanelBottomWindow->AddPage(m_transformPanel->getWxPtr(), "Transform");
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
  if (m_timer) {
    m_timer->Stop();
  }
}

} // namespace

wxFrame* createMainWindow(const wxString& title)
{
  return new MainWindow(title);
}

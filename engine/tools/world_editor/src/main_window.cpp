#include "mode_ui.hpp"
#include "editor_core.hpp"
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

class MainWindowImpl : public wxFrame
{
  public:
    explicit MainWindowImpl(const wxString& title);

    ~MainWindowImpl() override;

  private:
    void constructMenu();
    void constructLeftPanel();
    void constructRightPanel();

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

    bool ready() const;
    ModeUi& currentMode();

    wxSplitterWindow* m_splitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxPanel* m_rightSidePanel = nullptr;
    wxChoice* m_modeSelector = nullptr;
    wxNotebook* m_rightSidePanelTopWindow = nullptr;
    wxNotebook* m_rightSidePanelBottomWindow = nullptr;
    EditorCorePtr m_core;
    std::array<ModeUiPtr, 2> m_modes;
    wxPanel* m_canvas = nullptr;
    wxTimer* m_timer = nullptr;
    WindowDelegatePtr m_windowDelegate;
};

MainWindowImpl::MainWindowImpl(const wxString& title)
  : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition)
{
  Bind(wxEVT_CLOSE_WINDOW, &MainWindowImpl::onClose, this);

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

bool MainWindowImpl::ready() const
{
  return m_modes[0] != nullptr;
}

ModeUi& MainWindowImpl::currentMode()
{
  return *m_modes.at(m_modeSelector->GetSelection());
}

void MainWindowImpl::onOpen(wxCommandEvent&)
{
  auto dialog = new wxDirDialog{this, "Open directory"};

  if (dialog->ShowModal() != wxID_OK) {
    return;
  }

  m_windowDelegate = createWindowDelegate(m_canvas->GetHandle());
  m_core = createEditorCore(dialog->GetPath().ToStdString(), *m_windowDelegate);

  m_modes = {
    createSceneEditModeUi(*m_rightSidePanelTopWindow, *m_rightSidePanelBottomWindow,
      *m_core),
    createPrefabEditModeUi(*m_rightSidePanelTopWindow, *m_rightSidePanelBottomWindow,
      *m_core)
  };

  m_modes.at(0)->activate();

  m_modeSelector->SetSelection(0);
  m_modeSelector->Enable();

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);

  m_canvas->Bind(wxEVT_SIZE, &MainWindowImpl::onCanvasResize, this);
  m_canvas->Bind(wxEVT_KEY_DOWN, &MainWindowImpl::onCanvasKeyDown, this);
  m_canvas->Bind(wxEVT_KEY_UP, &MainWindowImpl::onCanvasKeyUp, this);
  m_canvas->Bind(wxEVT_LEFT_DOWN, &MainWindowImpl::onCanvasLeftMouseBtnDown, this);
  m_canvas->Bind(wxEVT_LEFT_UP, &MainWindowImpl::onCanvasLeftMouseBtnUp, this);
  m_canvas->Bind(wxEVT_MOTION, &MainWindowImpl::onCanvasMouseMove, this);

  Bind(wxEVT_TIMER, &MainWindowImpl::onTick, this);
}

void MainWindowImpl::onSave(wxCommandEvent&)
{
  if (!ready()) {
    return;
  }

  currentMode().saveChanges();
}

void MainWindowImpl::onClose(wxCloseEvent& e)
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

void MainWindowImpl::onCanvasKeyDown(wxKeyEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_core->onKeyDown(key);
  currentMode().onKeyDown(key);
}

void MainWindowImpl::onCanvasKeyUp(wxKeyEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  auto key = mapToLithic3dKey(e.GetKeyCode());
  m_core->onKeyUp(key);
  currentMode().onKeyUp(key);
}

void MainWindowImpl::onCanvasResize(wxSizeEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  m_core->onCanvasResize(e.GetSize().GetWidth(), e.GetSize().GetHeight());

#ifdef PLATFORM_OSX
  osxResizeMetalLayer(GetHandle(), e.GetSize().GetWidth(), e.GetSize().GetHeight());
#endif
}

void MainWindowImpl::onCanvasLeftMouseBtnDown(wxMouseEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  m_canvas->SetFocus();
  m_core->onMouseLeftBtnDown();
  currentMode().onMouseLeftBtnDown();
}

void MainWindowImpl::onCanvasLeftMouseBtnUp(wxMouseEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  m_core->onMouseLeftBtnUp();
  currentMode().onMouseLeftBtnUp();
}

void MainWindowImpl::onCanvasMouseMove(wxMouseEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  float x = static_cast<float>(e.GetX()) / GetClientSize().GetWidth();
  float y = static_cast<float>(e.GetY()) / GetClientSize().GetHeight();
  m_core->onMouseMove(x, y);
  currentMode().onMouseMove(x, y);
}

void MainWindowImpl::onTick(wxTimerEvent&)
{
  if (!ready()) {
    return;
  }

  m_core->update();
  currentMode().update();
}

void MainWindowImpl::constructMenu()
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

  Bind(wxEVT_MENU, &MainWindowImpl::onOpen, this, MNU_OPEN);
  Bind(wxEVT_MENU, &MainWindowImpl::onSave, this, MNU_SAVE);
  Bind(wxEVT_MENU, &MainWindowImpl::onExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &MainWindowImpl::onAbout, this, wxID_ABOUT);
}

void MainWindowImpl::constructLeftPanel()
{
  assert(m_splitter);

  m_leftPanel = new wxPanel(m_splitter, wxID_ANY);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_canvas = new wxPanel(m_leftPanel);
  vbox->Add(m_canvas, 1, wxEXPAND, 5);
  m_leftPanel->SetSizer(vbox);
  m_leftPanel->SetCanFocus(true);
}

void MainWindowImpl::constructRightPanel()
{
  m_rightSidePanel = new wxPanel(m_splitter);
  auto vbox = new wxBoxSizer(wxVERTICAL);
  m_rightSidePanel->SetSizer(vbox);

  wxFlexGridSizer* modeSizer = new wxFlexGridSizer(2);
  wxStaticText* modeLabel = new wxStaticText(m_rightSidePanel, wxID_ANY, "Mode");

  wxArrayString modes;
  modes.Add("Scene editor");
  modes.Add("Prefab editor");

  m_modeSelector = new wxChoice(m_rightSidePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    modes);

  modeSizer->Add(modeLabel, wxSizerFlags().Expand().Border());
  modeSizer->Add(m_modeSelector, wxSizerFlags().Expand().Border());
  modeSizer->AddGrowableCol(1);

  m_modeSelector->Disable();

  m_rightSidePanelTopWindow = new wxNotebook(m_rightSidePanel, wxID_ANY);
  m_rightSidePanelBottomWindow = new wxNotebook(m_rightSidePanel, wxID_ANY);

  vbox->Add(modeSizer, 0, wxEXPAND);
  vbox->Add(m_rightSidePanelTopWindow, 1, wxEXPAND);
  vbox->Add(m_rightSidePanelBottomWindow, 1, wxEXPAND);
}

void MainWindowImpl::onExit(wxCommandEvent&)
{
  Close();
}

void MainWindowImpl::onAbout(wxCommandEvent&)
{
  std::stringstream ss;
  ss << "Lithic3D World Editor." << std::endl << std::endl;
  ss << "Copyright Freehold Apps Ltd 2025 - 2026. All rights reserved.";

  wxMessageBox(wxGetTranslation(ss.str()), "Lithic3D World Editor", wxOK | wxICON_INFORMATION);
}

MainWindowImpl::~MainWindowImpl()
{
  if (m_timer) {
    m_timer->Stop();
  }

  if (m_core != nullptr) {
    m_core->engine().resourceManager().deactivate();
    for (auto& m : m_modes) {
      m.reset();
    }
    m_core.reset();
  }
}

} // namespace

wxFrame* createMainWindow(const wxString& title)
{
  return new MainWindowImpl(title);
}

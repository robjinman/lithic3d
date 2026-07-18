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
    void onCanvasMouseScroll(wxMouseEvent& e);

    void onModeChange(wxCommandEvent& e);

    bool ready() const;
    ModeUi& currentMode();

    wxSplitterWindow* m_outerSplitter = nullptr;
    wxSplitterWindow* m_innerSplitter = nullptr;
    wxBoxSizer* m_vbox = nullptr;
    wxPanel* m_leftPanel = nullptr;
    wxPanel* m_rightPanel = nullptr;
    wxPanel* m_rightSubpanel = nullptr;
    wxChoice* m_modeSelector = nullptr;
    EditorCorePtr m_core;
    std::array<ModeUiPtr, 2> m_modes;
    int m_currentMode = -1;
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

  SetAutoLayout(true);

  m_outerSplitter = new wxSplitterWindow(this);
  m_outerSplitter->SetMinimumPaneSize(300);

  m_innerSplitter = new wxSplitterWindow(m_outerSplitter);
  m_innerSplitter->SetMinimumPaneSize(300);

  m_vbox->Add(m_outerSplitter, wxSizerFlags(1).Expand());

  constructLeftPanel();
  m_canvas = new wxPanel(m_innerSplitter);
  m_canvas->SetCanFocus(true);
  constructRightPanel();

  m_outerSplitter->SplitVertically(m_leftPanel, m_innerSplitter, 300);
  m_innerSplitter->SplitVertically(m_canvas, m_rightPanel, -300);

  SetSizer(m_vbox);

  CreateStatusBar();
  SetStatusText(wxEmptyString);

  Maximize();
  Layout();
}

bool MainWindowImpl::ready() const
{
  return m_modes[0] != nullptr;
}

ModeUi& MainWindowImpl::currentMode()
{
  return *m_modes.at(m_currentMode);
}

void MainWindowImpl::onOpen(wxCommandEvent&)
{
  auto dialog = new wxDirDialog{this, "Open directory"};

  if (dialog->ShowModal() != wxID_OK) {
    return;
  }

  m_windowDelegate = createWindowDelegate(m_canvas->GetHandle());
  m_core = createEditorCore(dialog->GetPath().ToStdString(), *m_windowDelegate);

  Panels panels{
    .leftSidebar = m_leftPanel,
    .rightSidebar = m_rightSubpanel
  };

  m_modes = {
    createSceneEditModeUi(panels, *m_core),
    createEntityEditModeUi(panels, *m_core)
  };

  m_modes.at(0)->activate();

  m_modeSelector->SetSelection(0);
  m_modeSelector->Enable();

  m_currentMode = 0;

  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);

  m_canvas->Bind(wxEVT_SIZE, &MainWindowImpl::onCanvasResize, this);
  m_canvas->Bind(wxEVT_KEY_DOWN, &MainWindowImpl::onCanvasKeyDown, this);
  m_canvas->Bind(wxEVT_KEY_UP, &MainWindowImpl::onCanvasKeyUp, this);
  m_canvas->Bind(wxEVT_LEFT_DOWN, &MainWindowImpl::onCanvasLeftMouseBtnDown, this);
  m_canvas->Bind(wxEVT_LEFT_UP, &MainWindowImpl::onCanvasLeftMouseBtnUp, this);
  m_canvas->Bind(wxEVT_MOTION, &MainWindowImpl::onCanvasMouseMove, this);
  m_canvas->Bind(wxEVT_MOUSEWHEEL, &MainWindowImpl::onCanvasMouseScroll, this);

  Bind(wxEVT_TIMER, &MainWindowImpl::onTick, this);
}

void MainWindowImpl::onSave(wxCommandEvent&)
{
  if (!ready()) {
    return;
  }

  for (auto& mode : m_modes) {
    if (mode != nullptr) {
      mode->saveChanges();
    }
  }
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
    case WXK_CONTROL: return KeyboardKey::CtrlLeft;
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

void MainWindowImpl::onCanvasMouseScroll(wxMouseEvent& e)
{
  if (!ready()) {
    return;
  }

  e.Skip();

  m_core->onMouseScroll(e.GetWheelRotation() < 0.f);
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

void MainWindowImpl::onModeChange(wxCommandEvent&)
{
  currentMode().deactivate();
  m_currentMode = m_modeSelector->GetSelection();
  assert(m_currentMode >= 0 && static_cast<size_t>(m_currentMode) < m_modes.size());
  m_modes.at(m_currentMode)->activate();
}

void MainWindowImpl::constructLeftPanel()
{
  m_leftPanel = new wxPanel(m_outerSplitter, wxID_ANY);
  m_leftPanel->SetSizer(new wxBoxSizer(wxVERTICAL));
}

void MainWindowImpl::constructRightPanel()
{
  assert(m_innerSplitter);

  m_rightPanel = new wxPanel(m_innerSplitter);
  auto vbox = new wxBoxSizer(wxVERTICAL);

  wxFlexGridSizer* modeSizer = new wxFlexGridSizer(2);
  wxStaticText* modeLabel = new wxStaticText(m_rightPanel, wxID_ANY, "Mode");

  wxArrayString modes;
  modes.Add("Scene editor");
  modes.Add("Entity editor");

  m_modeSelector = new wxChoice(m_rightPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
    modes);

  modeSizer->Add(modeLabel, wxSizerFlags().Expand().Align(wxALIGN_CENTER_VERTICAL).Border());
  modeSizer->Add(m_modeSelector, wxSizerFlags(1).Expand().Border());
  modeSizer->AddGrowableCol(1);

  m_modeSelector->Disable();

  m_rightSubpanel = new wxPanel(m_rightPanel, wxID_ANY);
  m_rightSubpanel->SetSizer(new wxBoxSizer(wxVERTICAL));

  vbox->Add(modeSizer, wxSizerFlags().Expand());
  vbox->Add(m_rightSubpanel, wxSizerFlags(1).Expand());

  m_rightPanel->SetSizer(vbox);
  m_rightPanel->Layout();

  m_modeSelector->Bind(wxEVT_CHOICE, &MainWindowImpl::onModeChange, this);
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

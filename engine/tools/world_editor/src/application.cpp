#include "main_window.hpp"
#include <wx/wx.h>
#ifdef PLATFORM_LINUX
#include <X11/Xlib.h>
#endif

class Application : public wxApp
{
  public:
    virtual bool OnInit() override;
    virtual void HandleEvent(wxEvtHandler* handler, wxEventFunction func,
      wxEvent& event) const override;

  private:
    MainWindowPtr m_mainWindow;
};

bool Application::OnInit()
{
  m_mainWindow = createMainWindow("Lithic3D World Editor");
  m_mainWindow->getWxPtr()->Show();

  return true;
}

void Application::HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event) const
{
  try {
    wxApp::HandleEvent(handler, func, event);
  }
  catch (const std::runtime_error& e) {
    std::cerr << "A fatal exception occurred: " << std::endl;
    std::cerr << e.what() << std::endl;
    exit(1);
  }
}

#ifdef PLATFORM_LINUX
wxIMPLEMENT_APP_NO_MAIN(Application);

int main(int argc, char** argv)
{
  XInitThreads();
  return ::wxEntry(argc, argv);
}
#else
wxIMPLEMENT_APP(Application);
#endif

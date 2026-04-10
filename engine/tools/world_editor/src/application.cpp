#include <wx/wx.h>
#ifdef PLATFORM_LINUX
#include <X11/Xlib.h>
#endif

wxFrame* createMainWindow(const wxString& title);

namespace
{

void doWithLogging(std::function<void()> fn)
{
  try {
    fn();
  }
  catch (const std::runtime_error& e) {
    std::cerr << "A fatal exception occurred: " << std::endl;
    std::cerr << e.what() << std::endl;
    exit(1);
  }
}

} // namespace

class Application : public wxApp
{
  public:
    virtual bool OnInit() override;
    virtual void HandleEvent(wxEvtHandler* handler, wxEventFunction func,
      wxEvent& event) const override;
};

bool Application::OnInit()
{
  wxFrame* frame = createMainWindow("Lithic3D World Editor");
  frame->Show();

  return true;
}

void Application::HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event) const
{
  doWithLogging([this, handler, &func, &event]() {
    wxApp::HandleEvent(handler, func, event);
  });
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

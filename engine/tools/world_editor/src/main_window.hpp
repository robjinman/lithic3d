#pragma once

#include <wx/string.h>
#include <memory>

class wxFrame;

class MainWindow
{
  public:
    virtual wxFrame* getWxPtr() = 0;

    virtual ~MainWindow() = default;
};

using MainWindowPtr = std::unique_ptr<MainWindow>;

MainWindowPtr createMainWindow(const wxString& title);

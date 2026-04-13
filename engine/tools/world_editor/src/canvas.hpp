#pragma once

#include <wx/wx.h>
#include <filesystem>

class Canvas : public wxPanel
{
  public:
    Canvas(wxWindow* parent) : wxPanel(parent) {}

    virtual void startEngine(const std::filesystem::path& projectPath) = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual ~Canvas() = default;
};

Canvas* createCanvas(wxWindow* parent);

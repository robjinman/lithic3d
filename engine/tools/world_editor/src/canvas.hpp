#pragma once

#include <wx/wx.h>
#include <filesystem>

namespace lithic3d { class Engine; }

class Canvas : public wxPanel
{
  public:
    Canvas(wxWindow* parent) : wxPanel(parent) {}

    virtual void enable() = 0;
    virtual void disable() = 0;
    virtual void initialise(const std::filesystem::path& projectRoot) = 0;
    virtual lithic3d::Engine& engine() const = 0;

    virtual ~Canvas() = default;
};

Canvas* createCanvas(wxWindow* parent);

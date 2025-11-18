#pragma once

#include <wx/wx.h>

class Canvas : public wxPanel
{
  public:
    Canvas(wxWindow* parent) : wxPanel(parent) {}

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual ~Canvas() = default;
};

Canvas* createCanvas(wxWindow* parent);

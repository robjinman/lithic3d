#pragma once

#include <wx/wx.h>

enum class Tool : int
{
  BoundingBox
};

wxDECLARE_EVENT(EToolToggleOn, wxCommandEvent);
wxDECLARE_EVENT(EToolToggleOff, wxCommandEvent);

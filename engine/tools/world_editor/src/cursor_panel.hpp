#pragma once

#include <wx/wx.h>
#include <memory>

class EditorCore;

class CursorPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;

    virtual ~CursorPanel() = default;
};

using CursorPanelPtr = std::unique_ptr<CursorPanel>;

wxDECLARE_EVENT(ECancelActiveTransform, wxCommandEvent);
wxDECLARE_EVENT(EApplyActiveTransform, wxCommandEvent);

CursorPanelPtr createCursorPanel(wxWindow* parent, EditorCore& editorCore);

#pragma once

#include <memory>

class wxPanel;
class wxWindow;
class EditorCore;

class CurrentTransformPanel
{
  public:
    virtual wxPanel* getWxPtr() = 0;

    virtual ~CurrentTransformPanel() = default;
};

using CurrentTransformPanelPtr = std::unique_ptr<CurrentTransformPanel>;

CurrentTransformPanelPtr createCurrentTransformPanel(wxWindow* parent, EditorCore& editorCore);

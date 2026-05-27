#pragma once

#include <memory>

class wxWindow;
class EditorCore;

class CurrentTransformPanel
{
  public:
    virtual wxWindow* getWxPtr() = 0;

    virtual ~CurrentTransformPanel() = default;
};

using CurrentTransformPanelPtr = std::unique_ptr<CurrentTransformPanel>;

CurrentTransformPanelPtr createCurrentTransformPanel(wxWindow* parent, EditorCore& editorCore);

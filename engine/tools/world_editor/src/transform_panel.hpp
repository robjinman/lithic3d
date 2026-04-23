#pragma once

#include <memory>

class wxPanel;
class wxWindow;
class WorldEditor;

class TransformPanel
{
  public:
    virtual wxPanel* getWxPtr() = 0;

    virtual ~TransformPanel() = default;
};

using TransformPanelPtr = std::unique_ptr<TransformPanel>;

TransformPanelPtr createTransformPanel(wxWindow* parent, WorldEditor& worldEditor);

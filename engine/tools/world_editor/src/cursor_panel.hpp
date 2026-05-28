#pragma once

#include "event_emitter.hpp"
#include <memory>

class wxWindow;
class EditorCore;

class CursorPanel
{
  public:
    enum class Event : EventId
    {
      Apply,
      Cancel
    };

    virtual wxWindow* getWxPtr() = 0;
    virtual EventHandle listen(Event eventId, const EventHandler& handler) = 0;

    virtual ~CursorPanel() = default;
};

using CursorPanelPtr = std::unique_ptr<CursorPanel>;

CursorPanelPtr createCursorPanel(wxWindow* parent, EditorCore& editorCore);

#include "canvas.hpp"
#include <lithic3d/lithic3d.hpp>

namespace lithic3d
{

PlatformPathsPtr createPlatformPaths(const std::string& appName, const std::string& vendorName);
FileSystemPtr createDefaultFileSystem(PlatformPathsPtr platformPaths);

} // namespace lithic3d

using namespace lithic3d;

WindowDelegatePtr createWindowDelegate(WXWidget window);

using namespace lithic3d;

namespace
{

class CanvasImpl : public Canvas
{
  public:
    CanvasImpl(wxWindow* parent);

    void enable() override;
    void disable() override;

  private:
    void initialise();
    wxPoint getCursorPos() const;

    void onResize(wxSizeEvent& e);
    void onKeyPress(wxKeyEvent& e);
    void onLeftMouseBtnDown(wxMouseEvent& e);
    void onLeftMouseBtnUp(wxMouseEvent& e);
    void onMouseMove(wxMouseEvent& e);
    void onPaint(wxPaintEvent& e);
    void onTick(wxTimerEvent& e);

    EnginePtr m_engine;
    bool m_disabled = false;
    wxTimer* m_timer = nullptr;
    bool m_mouseDown = false;
    wxRect m_selectionRect;
};

CanvasImpl::CanvasImpl(wxWindow* parent)
  : Canvas(parent)
{
  m_timer = new wxTimer(this);
  m_timer->Start(1000.0 / TICKS_PER_SECOND);

  Bind(wxEVT_PAINT, &CanvasImpl::onPaint, this);
  Bind(wxEVT_SIZE, &CanvasImpl::onResize, this);
  Bind(wxEVT_KEY_DOWN, &CanvasImpl::onKeyPress, this);
  Bind(wxEVT_LEFT_DOWN, &CanvasImpl::onLeftMouseBtnDown, this);
  Bind(wxEVT_LEFT_UP, &CanvasImpl::onLeftMouseBtnUp, this);
  Bind(wxEVT_MOTION, &CanvasImpl::onMouseMove, this);
  Bind(wxEVT_TIMER, &CanvasImpl::onTick, this);
}

void CanvasImpl::initialise()
{
  if (m_engine == nullptr) {
    auto platformPaths = createPlatformPaths("lithic3d_world_editor", "freeholdapps");
    auto fileSystem = createDefaultFileSystem(std::move(platformPaths));
    auto windowDelegate = createWindowDelegate(GetHandle());
    auto logger = createLogger(std::cerr, std::cerr, std::cout, std::cout);
    auto audioSystem = createAudioSystem(*fileSystem);
    auto renderer = createRenderer(std::move(windowDelegate), *fileSystem, *logger, {});

    m_engine = createEngine(std::move(renderer), std::move(audioSystem), std::move(fileSystem),
      std::move(logger));

    m_engine->renderer().start();
  }
}

void CanvasImpl::onTick(wxTimerEvent&)
{
  Refresh(false);
}

void CanvasImpl::onResize(wxSizeEvent& e)
{
  e.Skip();

  // TODO: On OSX, resize metal layer

  if (m_engine != nullptr) {
    m_engine->onWindowResize();
  }
}

wxPoint CanvasImpl::getCursorPos() const
{
  auto pt = wxGetMousePosition();
  return pt - GetScreenPosition();
}

void CanvasImpl::onLeftMouseBtnDown(wxMouseEvent& e)
{
  e.Skip();

  SetFocus();

  m_mouseDown = true;
  m_selectionRect = wxRect(e.GetPosition(), wxSize(0, 0));
}

void CanvasImpl::onLeftMouseBtnUp(wxMouseEvent& e)
{
  e.Skip();

  m_mouseDown = false;

  auto selectionSizeAboveThreshold = [](wxSize sz) {
    return sz.x * sz.x + sz.y * sz.y >= 64;
  };

  if (selectionSizeAboveThreshold(m_selectionRect.GetSize())) {
    int x0 = m_selectionRect.x;
    int y0 = m_selectionRect.y;
    int x1 = x0 + m_selectionRect.width;
    int y1 = y0 + m_selectionRect.height;

    // TODO
  }
}

void CanvasImpl::onMouseMove(wxMouseEvent& e)
{
  e.Skip();

  if (m_mouseDown) {
    wxPoint p = wxGetMousePosition() - GetScreenPosition();
    wxPoint sz_p = p - m_selectionRect.GetTopLeft();
    wxSize sz(sz_p.x, sz_p.y);

    wxSize winSz = GetClientSize();
    float aspect = static_cast<float>(winSz.x) / static_cast<float>(winSz.y);
    sz.y = sz.x / aspect;

    m_selectionRect.SetSize(sz);
  }
}

void CanvasImpl::onKeyPress(wxKeyEvent& e)
{
  e.Skip();

  auto key = e.GetKeyCode();

  // TODO
}

void CanvasImpl::onPaint(wxPaintEvent&)
{
  wxPaintDC dc(this);

  if (m_engine == nullptr) {
    initialise();
    return;
  }

  if (m_engine != nullptr) {
    m_engine->update({});
  }

  Refresh(false);
}

void CanvasImpl::disable()
{
  m_disabled = true;
  Disable();
}

void CanvasImpl::enable()
{
  m_disabled = false;
  Enable();
}

} // namespace

Canvas* createCanvas(wxWindow* parent)
{
  return new CanvasImpl(parent);
}

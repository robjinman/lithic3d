#include "mobile_controls.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "sys_animation.hpp"
#include "sys_ui.hpp"
#include "systems.hpp"

namespace
{

const uint32_t zIndex = 199;

class MobileControlsImpl : public MobileControls
{
  public:
    MobileControlsImpl(Ecs& ecs, EventSystem& eventSystem,
      const MobileControlsCallbacks& callbacks, float screenAspect, float gameAreaAspect);

    void hide() override;
    void show() override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    MobileControlsCallbacks m_callbacks;
    EntityId m_rootId;

    EntityId constructRoot();
    void constructButton(const Vec2f& pos, const Vec2f& size, const std::function<void()>& onPress,
      const std::function<void()>& onRelease);
};

MobileControlsImpl::MobileControlsImpl(Ecs& ecs, EventSystem& eventSystem,
  const MobileControlsCallbacks& callbacks, float screenAspect, float gameAreaAspect)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_callbacks(callbacks)
{
  float gameAreaW = gameAreaAspect;
  float screenW = screenAspect;
  float margin = (screenW - gameAreaW) / 2.f;
  float gap = 0.04f;  // Distance between controls and game area or edge of screen
  float btnW = (margin - 2.f * gap) / 3.f;
  float btnH = btnW;
  Vec2f btnSz{ btnW, btnH };
  float x0 = -margin + gap;
  float y0 = gap;

  Vec2f btnLeftPos{ x0 + 0.f * btnW, y0 + 1.f * btnH };
  Vec2f btnRightPos{ x0 + 2.f * btnW, y0 + 1.f * btnH };
  Vec2f btnUpPos{ x0 + 1.f * btnW, y0 + 2.f * btnH };
  Vec2f btnDownPos{ x0 + 1.f * btnW, y0 + 0.f * btnH };

  m_rootId = constructRoot();
  constructButton(btnLeftPos, btnSz, m_callbacks.onLeftButtonPress,
    m_callbacks.onLeftButtonRelease);
  constructButton(btnRightPos, btnSz, m_callbacks.onRightButtonPress,
    m_callbacks.onRightButtonRelease);
  constructButton(btnUpPos, btnSz, m_callbacks.onUpButtonPress, m_callbacks.onUpButtonRelease);
  constructButton(btnDownPos, btnSz, m_callbacks.onDownButtonPress,
    m_callbacks.onDownButtonRelease);

  Vec2f btnActionPos{ gameAreaW + gap, y0 };
  constructButton(btnActionPos, btnSz, m_callbacks.onActionButtonPress,
    m_callbacks.onActionButtonRelease);

  Vec2f btnEscapePos{ gameAreaW + gap, 1.f - btnH - gap };
  constructButton(btnEscapePos, btnSz, m_callbacks.onEscapeButtonPress,
    m_callbacks.onEscapeButtonRelease);
}

void MobileControlsImpl::show()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  sysSpatial.setEnabled(m_rootId, true);
}

void MobileControlsImpl::hide()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  sysSpatial.setEnabled(m_rootId, false);
}

EntityId MobileControlsImpl::constructRoot()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  return id;
}

void MobileControlsImpl::constructButton(const Vec2f& pos, const Vec2f& size,
  const std::function<void()>& onPress, const std::function<void()>& onRelease)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  const Vec4f colourPressed{ 0.15f, 0.1f, 0.1f, 1.f };
  const Vec4f colourUnpressed{ 0.1f, 0.1f, 0.15f, 1.f };

  auto id = m_ecs.componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CRender, CUi
  >();

  SpatialData spatial{
    .transform = translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
      scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f }),
    .parent = m_rootId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  QuadData render{
    .viewport = MOBILE_CONTROLS_VIEWPORT,
    .zIndex = zIndex,
    .colour = colourUnpressed
  };

  sysRender.addEntity(id, render);

  UiData ui{};
  ui.inputFilter = { MouseButton::Left };
  ui.onInputBegin = [&sysRender, &onPress, colourPressed, id](const UserInput&) {
    sysRender.setColour(id, colourPressed);
    onPress();
  };
  ui.onInputEnd = [&sysRender, &onRelease, colourUnpressed, id](const UserInput&) {
    sysRender.setColour(id, colourUnpressed);
    onRelease();
  };
  ui.onInputCancel = [&sysRender, &onRelease, colourUnpressed, id]() {
    sysRender.setColour(id, colourUnpressed);
    onRelease();
  };

  sysUi.addEntity(id, ui);
}

} // namespace

MobileControlsPtr createMobileControls(Ecs& ecs, EventSystem& eventSystem,
  const MobileControlsCallbacks& callbacks, float screenAspect, float gameAreaAspect)
{
  return std::make_unique<MobileControlsImpl>(ecs, eventSystem, callbacks, screenAspect,
    gameAreaAspect);
}

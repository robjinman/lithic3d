#include "mobile_controls.hpp"
#include "units.hpp"
#include <fge/sys_render.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/sys_animation.hpp>
#include <fge/sys_ui.hpp>
#include <fge/systems.hpp>

using fge::EntityId;
using fge::Vec2f;
using fge::Vec3f;
using fge::Vec4f;
using fge::Rectf;
using fge::identityMatrix;
using fge::Ecs;
using fge::EventSystem;
using fge::SpatialData;
using fge::TextData;
using fge::UiData;
using fge::QuadData;
using fge::CLocalTransform;
using fge::CSpatialFlags;
using fge::CGlobalTransform;
using fge::CSprite;
using fge::CUi;
using fge::CRender;
using fge::SysSpatial;
using fge::SysRender;
using fge::SysUi;
using fge::SPATIAL_SYSTEM;
using fge::RENDER_SYSTEM;
using fge::UI_SYSTEM;
using fge::UserInput;
using fge::MouseButton;

namespace
{

const uint32_t zIndex = 199;

class MobileControlsImpl : public MobileControls
{
  public:
    MobileControlsImpl(Ecs& ecs, EventSystem& eventSystem,
      const MobileControlsCallbacks& callbacks, const Rectf& gameArea);

    void hide() override;
    void show() override;
    void setGameArea(const Rectf& gameArea) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    MobileControlsCallbacks m_callbacks;
    EntityId m_rootId;
    EntityId m_leftBtnId;
    EntityId m_rightBtnId;
    EntityId m_upBtnId;
    EntityId m_downBtnId;
    EntityId m_actionBtnId;
    EntityId m_escapeBtnId;

    EntityId constructRoot();
    EntityId constructButton(const std::function<void()>& onPress,
      const std::function<void()>& onRelease, const std::string& label);
    EntityId constructButtonLabel(const std::string& label, EntityId parentId);
    void positionButtons(const Rectf& gameArea);
};

MobileControlsImpl::MobileControlsImpl(Ecs& ecs, EventSystem& eventSystem,
  const MobileControlsCallbacks& callbacks, const Rectf& gameArea)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_callbacks(callbacks)
{
  m_rootId = constructRoot();
  m_leftBtnId = constructButton(m_callbacks.onLeftButtonPress, m_callbacks.onLeftButtonRelease, "");
  m_rightBtnId = constructButton(m_callbacks.onRightButtonPress, m_callbacks.onRightButtonRelease,
    "");
  m_upBtnId = constructButton(m_callbacks.onUpButtonPress, m_callbacks.onUpButtonRelease, "");
  m_downBtnId = constructButton(m_callbacks.onDownButtonPress, m_callbacks.onDownButtonRelease, "");
  m_actionBtnId = constructButton(m_callbacks.onActionButtonPress,
    m_callbacks.onActionButtonRelease, "A");
  m_escapeBtnId = constructButton(m_callbacks.onEscapeButtonPress,
    m_callbacks.onEscapeButtonRelease, "B");

  positionButtons(gameArea);
}

void MobileControlsImpl::positionButtons(const Rectf& gameArea)
{
  float margin = gameArea.x;
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
  Vec2f btnActionPos{ gameArea.w + gap + 1.f * btnW, y0 };
  Vec2f btnEscapePos{ gameArea.w + gap + 2.f * btnW, y0 + btnH };

  auto setTransform = [this](EntityId id, const Vec2f& pos, const Vec2f& size) {
    m_ecs.componentStore().component<CLocalTransform>(id).transform =
      fge::translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
      fge::scaleMatrix4x4(Vec3f{ size[0], size[1], 1.f });
  };

  setTransform(m_leftBtnId, btnLeftPos, btnSz);
  setTransform(m_rightBtnId, btnRightPos, btnSz);
  setTransform(m_upBtnId, btnUpPos, btnSz);
  setTransform(m_downBtnId, btnDownPos, btnSz);
  setTransform(m_actionBtnId, btnActionPos, btnSz);
  setTransform(m_escapeBtnId, btnEscapePos, btnSz);
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

void MobileControlsImpl::setGameArea(const Rectf& gameArea)
{
  positionButtons(gameArea);
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

EntityId MobileControlsImpl::constructButtonLabel(const std::string& label, EntityId parentId)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  auto labelId = m_ecs.componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CRender, CSprite
  >();

  SpatialData spatial{
    .transform = translationMatrix4x4(Vec3f{ 0.25f, 0.25f, 0.f })
      * scaleMatrix4x4(Vec3f{ 0.5f, 0.5f, 1.f }),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(labelId, spatial);

  TextData render{
    .scissor = MOBILE_CONTROLS_SCISSOR,
    .textureRect = {
      .x = fge::pxToUvX(256.f),
      .y = fge::pxToUvY(64.f, 192.f),
      .w = fge::pxToUvW(192.f),
      .h = fge::pxToUvH(192.f)
    },
    .text = label,
    .zIndex = zIndex + 1,
    .colour = { 0.2f, 0.9f, 0.2f, 1.f }
  };

  sysRender.addEntity(labelId, render);

  return labelId;
}

EntityId MobileControlsImpl::constructButton(const std::function<void()>& onPress,
  const std::function<void()>& onRelease, const std::string& label)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  const Vec4f colourPressed{ 0.25f, 0.f, 0.f, 1.f };
  const Vec4f colourUnpressed{ 0.1f, 0.1f, 0.2f, 1.f };

  auto id = m_ecs.componentStore().allocate<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CRender, CUi
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_rootId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  QuadData render{
    .scissor = MOBILE_CONTROLS_SCISSOR,
    .zIndex = zIndex,
    .colour = colourUnpressed
  };

  sysRender.addEntity(id, render);

  UiData ui{};
  ui.canReceiveFocus = false;
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

  if (!label.empty()) {
    constructButtonLabel(label, id);
  }

  return id;
}

} // namespace

MobileControlsPtr createMobileControls(Ecs& ecs, EventSystem& eventSystem,
  const MobileControlsCallbacks& callbacks, const Rectf& gameArea)
{
  return std::make_unique<MobileControlsImpl>(ecs, eventSystem, callbacks, gameArea);
}

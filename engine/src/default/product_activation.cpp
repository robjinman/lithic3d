#include <lithic3d/product_activation.hpp>
#include <lithic3d/drm.hpp>
#include <lithic3d/logger.hpp>
#include <lithic3d/sys_spatial.hpp>
#include <lithic3d/systems.hpp>
#include <lithic3d/sys_render_2d.hpp>
#include <lithic3d/sys_ui.hpp>

namespace lithic3d
{
namespace
{

const std::set<UserInput> hexFilter{
  KeyboardKey::A, KeyboardKey::B, KeyboardKey::C, KeyboardKey::D, KeyboardKey::E, KeyboardKey::F,
  KeyboardKey::Num0, KeyboardKey::Num1, KeyboardKey::Num2, KeyboardKey::Num3, KeyboardKey::Num4,
  KeyboardKey::Num5, KeyboardKey::Num6, KeyboardKey::Num7, KeyboardKey::Num8, KeyboardKey::Num9
};

class ProductActivationImpl : public ProductActivation
{
  public:
    ProductActivationImpl(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
      const render::BitmapFont& font, Logger& logger);

    EntityId root() const override;

  private:
    Logger& m_logger;
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    Drm& m_drm;
    render::BitmapFont m_font;
    EntityId m_rootId;

    EntityId constructRoot();
    EntityId constructPrompt();
    EntityId constructTextbox();
};

ProductActivationImpl::ProductActivationImpl(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  const render::BitmapFont& font, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_drm(drm)
  , m_font(font)
{
  m_rootId = constructRoot();
  constructPrompt();
  constructTextbox();
}

EntityId ProductActivationImpl::root() const
{
  return m_rootId;
}

EntityId ProductActivationImpl::constructRoot()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();

  auto id = m_ecs.componentStore().allocate<DSpatial>();

  DSpatial spatial{
    .transform = identityMatrix<4>(),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  return id;
}

EntityId ProductActivationImpl::constructPrompt()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();

  auto id = m_ecs.componentStore().allocate<DSpatial, DText>();

  Vec2f pos{ 0.3f, 0.7f };
  Vec2f size{ 0.022f, 0.044f };

  DSpatial spatial{
    .transform = translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
      scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f }),
    .parent = m_rootId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DText render{
    .scissor = 0,
    .material = m_font.material,
    .textureRect = m_font.textureSection,
    .text = "Enter 8-digit activation key",
    .zIndex = 1,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  sysRender.addEntity(id, render);

  return id;
}

struct CTextbox
{
  uint32_t cursorPos = 0;

  static constexpr ComponentType TypeId = CTextboxTypeId;
};

EntityId ProductActivationImpl::constructTextbox()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender2d>();
  auto& sysUi = m_ecs.system<SysUi>();

  auto& store = m_ecs.componentStore();

  auto id = store.allocate<DSpatial, DDynamicText, DUi>();

  store.component<CTextbox>(id) = CTextbox{};

  Vec2f pos{ 0.3f, 0.5f };
  Vec2f size{ 0.05f, 0.1f };

  DSpatial spatial{
    .transform = translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
      scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f }),
    .parent = m_rootId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  DDynamicText render{
    .scissor = 0,
    .material = m_font.material,
    .textureRect = m_font.textureSection,
    .text = "________",
    .maxLength = 8,
    .zIndex = 1,
    .colour = { 1.f, 1.f, 1.f, 1.f }
  };

  sysRender.addEntity(id, render);

  DUi ui{};
  ui.group = SysUi::nextGroupId();
  ui.canReceiveFocus = true;
  ui.inputFilter = hexFilter,
  ui.inputFilter.insert(KeyboardKey::Backspace);
  ui.onInputBegin = [this, id, &sysRender, &store](const UserInput& input) {
    auto key = std::get<KeyboardKey>(input);
    auto& textbox = store.component<CTextbox>(id);

    if (key == KeyboardKey::Backspace) {
      if (textbox.cursorPos > 0) {
        --textbox.cursorPos;
        auto& dynText = store.component<CDynamicText>(id);
        std::string text{dynText.text, 8};
        text[textbox.cursorPos] = '_';

        sysRender.updateDynamicText(id, text);
      }
    }
    else {
      if (textbox.cursorPos < 8) {
        char character = 'A' + (static_cast<uint32_t>(key) - static_cast<uint32_t>(KeyboardKey::A));

        auto& dynText = store.component<CDynamicText>(id);
        std::string text{dynText.text, 8};
        text[textbox.cursorPos] = character;

        sysRender.updateDynamicText(id, text);

        if (++textbox.cursorPos == 8) {
          if (m_drm.activate(text)) {
            m_eventSystem.raiseEvent(EProductActivate{});
          }
        }
      }
    }
  };

  sysUi.addEntity(id, ui);
  sysUi.sendFocus(id);

  return id;
}

} // namespace

ProductActivationPtr createProductActivation(Ecs& ecs, EventSystem& eventSystem, Drm& drm,
  const render::BitmapFont& font, Logger& logger)
{
  return std::make_unique<ProductActivationImpl>(ecs, eventSystem, drm, font, logger);
}

} // namespace lithic3d

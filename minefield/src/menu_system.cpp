#include "menu_system.hpp"
#include "game_options.hpp"
#include "utils.hpp"
#include "units.hpp"
#include "sys_grid.hpp"
#include <fge/b_generic.hpp>
#include <fge/ecs.hpp>
#include <fge/event_system.hpp>
#include <fge/logger.hpp>
#include <fge/sys_render.hpp>
#include <fge/sys_spatial.hpp>
#include <fge/sys_behaviour.hpp>
#include <fge/sys_animation.hpp>
#include <fge/sys_ui.hpp>
#include <fge/systems.hpp>
#include <fge/input.hpp>
#include <cstring>

using fge::EntityId;
using fge::NULL_ENTITY;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::EventSystem;
using fge::Ecs;
using fge::Vec2f;
using fge::Vec3f;
using fge::Vec4f;
using fge::Rectf;
using fge::Mat4x4f;
using fge::identityMatrix;
using fge::SysUi;
using fge::SysAnimation;
using fge::SysRender;
using fge::SysSpatial;
using fge::SysBehaviour;
using fge::AnimationId;
using fge::Animation;
using fge::AnimationFrame;
using fge::SpriteData;
using fge::SpatialData;
using fge::DynamicTextData;
using fge::QuadData;
using fge::TextData;
using fge::UiData;
using fge::AnimationData;
using fge::CLocalTransform;
using fge::CGlobalTransform;
using fge::CSpatialFlags;
using fge::CDynamicText;
using fge::CQuad;
using fge::CRender;
using fge::CSprite;
using fge::CUi;
using fge::KeyboardKey;
using fge::MouseButton;
using fge::UserInput;
using fge::pxToUvX;
using fge::pxToUvY;
using fge::pxToUvW;
using fge::pxToUvH;

namespace
{

const uint32_t MAX_DIFFICULTY_LEVEL = 4;

enum class ZIndex : uint32_t
{
  Backdrop,
  Flare,
  MenuItem
};

static const HashedString strIdle = hashString("idle");
static const HashedString strIdleFocused = hashString("idle_focused");
static const HashedString strPrime = hashString("prime");
static const HashedString strActivate = hashString("activate");
static const HashedString strCancel = hashString("cancel");

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size)
{
  return fge::translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    fge::scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

struct Menu
{
  EntityId entityId = NULL_ENTITY;
  SysUi::GroupId itemGroupId = 0;
  EntityId firstItemId = NULL_ENTITY;
};

struct GameOptionCounters
{
  EntityId minesId = NULL_ENTITY;
  EntityId totalGoldId = NULL_ENTITY;
  EntityId sticksId = NULL_ENTITY;
  EntityId wanderersId = NULL_ENTITY;
  EntityId goldRequiredId = NULL_ENTITY;
  EntityId timeAvailableId = NULL_ENTITY;
  EntityId bestTimeId = NULL_ENTITY;
};

struct Slider
{
  EntityId entityId = NULL_ENTITY;
  float height;
};

struct SelectorFunctions
{
  std::function<void()> leftBtnDown = []() {};
  std::function<void()> leftBtnUp = []() {};
  std::function<void()> rightBtnDown = []() {};
  std::function<void()> rightBtnUp = []() {};
};

struct ItemSlots
{
  EntityId top = NULL_ENTITY;
  EntityId bottom = NULL_ENTITY;
};

struct Sprite
{
  Vec2f pos;
  Vec2f size;
  Rectf texRect;
};

class MenuSystemImpl : public MenuSystem
{
  public:
    MenuSystemImpl(fge::Ecs& ecs, fge::EventSystem& eventSystem, const GameOptionsManager& options,
      fge::Logger& logger, bool hasQuitButton);

    EntityId root() const override;

    EntityId startGameBtn() const override;
    EntityId resumeBtn() const override;
    EntityId quitToMainMenuBtn() const override;
    EntityId quitGameBtn() const override;

    float sfxVolume() const override;
    float musicVolume() const override;
    uint32_t difficultyLevel() const override;

    void showPauseMenu() override;
    void showMainMenu() override;

    void update() override;

  private:
    fge::Ecs& m_ecs;
    fge::EventSystem& m_eventSystem;
    const GameOptionsManager& m_options;
    fge::Logger& m_logger;
    EntityId m_root;
    EntityId m_pauseMenuId;
    Menu m_pauseMenuMain;
    EntityId m_pauseMenuSettingsId;
    Menu m_mainMenu;
    EntityId m_startGameBtnId;
    EntityId m_resumeBtnId;
    EntityId m_quitToMainBtnId;
    EntityId m_quitGameBtnId;
    AnimationId m_animIdle;
    AnimationId m_animIdleFocused;
    AnimationId m_animPrime;
    AnimationId m_animActivate;

    // Because settings menu is duplicated
    Slider m_musicVolumeSlider1;
    Slider m_sfxVolumeSlider1;
    Slider m_musicVolumeSlider2;
    Slider m_sfxVolumeSlider2;

    Slider m_gameDifficultySlider;

    GameOptionCounters m_gameOptionCounters;

    float m_sfxVolume = 0.75f;
    float m_musicVolume = 0.75f;
    uint32_t m_difficultyLevel = 0;
    float m_musicVolumeDelta = 0.f;
    float m_sfxVolumeDelta = 0.f;

    void constructRoot();
    void constructBackdrop();
    void constructFlare();
    void constructPauseMenu();
    void constructMainMenu(bool hasQuitButton);
    Menu constructSettingsSubmenu(EntityId parentId, const Menu& prevMenu);
    Menu constructCreditsSubmenu(const Menu& prevMenu);
    Menu constructGameOptionsSubmenu(const Menu& prevMenu);
    void createAnimations();
    EntityId newMenuItemId();
    void setDifficulty(int level);
    Slider constructSlider(EntityId parentId, const Vec2f& pos, float initialValue);
    EntityId constructQuad(EntityId parentId, const Vec2f& pos, const Vec2f& size,
      const Vec4f& colour);
    void constructMenuItemBase(EntityId id, EntityId parentId, const Sprite& sprite);
    void constructMenuItem(EntityId id, EntityId parentId, SysUi::GroupId groupId,
      const Sprite& sprite, const ItemSlots& slots);
    void constructSelector(EntityId id, EntityId parentId, SysUi::GroupId groupId,
      const Sprite& sprite, const ItemSlots& slots, const SelectorFunctions& functions);
    EntityId constructGameOptionCounter(EntityId parentId, const Vec2f& pos,
      const std::string& text, uint32_t value);
    EntityId constructTextItem(EntityId parentId, const Vec2f& pos, const Vec2f& charSize,
      const std::string& text, const Vec4f& colour);
    void addFadeInAnimForEntity(EntityId entityId);
    void updateSlider(const Slider& slider, float value);
};

MenuSystemImpl::MenuSystemImpl(fge::Ecs& ecs, fge::EventSystem& eventSystem,
  const GameOptionsManager& options, fge::Logger& logger, bool hasQuitButton)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_options(options)
  , m_logger(logger)
{
  createAnimations();

  constructRoot();
  constructBackdrop();
  constructFlare();
  constructPauseMenu();
  constructMainMenu(hasQuitButton);
}

EntityId MenuSystemImpl::startGameBtn() const
{
  return m_startGameBtnId;
}

EntityId MenuSystemImpl::resumeBtn() const
{
  return m_resumeBtnId;
}

EntityId MenuSystemImpl::quitToMainMenuBtn() const
{
  return m_quitToMainBtnId;
}

EntityId MenuSystemImpl::quitGameBtn() const
{
  return m_quitGameBtnId;
}

float MenuSystemImpl::sfxVolume() const
{
  return m_sfxVolume;
}

float MenuSystemImpl::musicVolume() const
{
  return m_musicVolume;
}

uint32_t MenuSystemImpl::difficultyLevel() const
{
  return m_difficultyLevel;
}

void MenuSystemImpl::setDifficulty(int level)
{
  auto& sysRender = m_ecs.system<SysRender>();

  m_difficultyLevel = level;

  float value = static_cast<float>(level) / MAX_DIFFICULTY_LEVEL;
  updateSlider(m_gameDifficultySlider, value);

  GameOptions options = m_options.getOptionsForLevel(level);

  uint32_t totalGold = options.coins + 5 * options.nuggets;

  std::string bestTimeString = options.bestTime == std::numeric_limits<uint32_t>::max() ?
    "---" : std::to_string(options.bestTime);

  const auto& counters = m_gameOptionCounters;
  sysRender.updateDynamicText(counters.minesId, std::to_string(options.mines));
  sysRender.updateDynamicText(counters.totalGoldId, std::to_string(totalGold));
  sysRender.updateDynamicText(counters.sticksId, std::to_string(options.sticks));
  sysRender.updateDynamicText(counters.wanderersId, std::to_string(options.wanderers));
  sysRender.updateDynamicText(counters.goldRequiredId, std::to_string(options.goldRequired));
  sysRender.updateDynamicText(counters.timeAvailableId, std::to_string(options.timeAvailable));
  sysRender.updateDynamicText(counters.bestTimeId, bestTimeString);
}

void MenuSystemImpl::updateSlider(const Slider& slider, float value)
{
  auto& t = m_ecs.componentStore().component<fge::CLocalTransform>(slider.entityId);
  t.transform.set(1, 1, value * slider.height);
}

void MenuSystemImpl::update()
{
  if (m_musicVolumeDelta != 0.f) {
    m_musicVolume = fge::clip(m_musicVolume + m_musicVolumeDelta, 0.f, 1.f);
    updateSlider(m_musicVolumeSlider1, m_musicVolume);
    updateSlider(m_musicVolumeSlider2, m_musicVolume);
  }

  if (m_sfxVolumeDelta != 0.f) {
    m_sfxVolume = fge::clip(m_sfxVolume + m_sfxVolumeDelta, 0.f, 1.f);
    updateSlider(m_sfxVolumeSlider1, m_sfxVolume);
    updateSlider(m_sfxVolumeSlider2, m_sfxVolume);
  }
}

void MenuSystemImpl::createAnimations()
{
  auto& sysAnimation = m_ecs.system<SysAnimation>();

  auto makeFrame = [](const Vec4f& colour) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = std::nullopt,
      .colour = colour
    };
  };

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = strIdle,
    .duration = 20,
    .frames = {
      makeFrame({ 1.f, 1.f, 1.f, 1.f }),
      makeFrame({ 0.8f, 0.8f, 0.8f, 1.f }),
      makeFrame({ 0.6f, 0.6f, 0.8f, 1.f }),
      makeFrame({ 0.8f, 0.8f, 0.8f, 1.f })
    }
  });

  m_animIdle = sysAnimation.addAnimation(std::move(animIdle));

  auto animIdleFocused = std::unique_ptr<Animation>(new Animation{
    .name = strIdleFocused,
    .duration = 20,
    .frames = {
      makeFrame({ 0.9f, 0.45f, 0.45f, 1.f }),
      makeFrame({ 0.8f, 0.35f, 0.35f, 1.f }),
      makeFrame({ 0.6f, 0.25f, 0.25f, 1.f }),
      makeFrame({ 0.8f, 0.35f, 0.35f, 1.f })
    }
  });

  m_animIdleFocused = sysAnimation.addAnimation(std::move(animIdleFocused));

  auto animPrime = std::unique_ptr<Animation>(new Animation{
    .name = strPrime,
    .duration = 1,
    .frames = {
      makeFrame({ 0.4f, 0.f, 0.f, 1.f })
    }
  });

  m_animPrime = sysAnimation.addAnimation(std::move(animPrime));

  auto animActivate = std::unique_ptr<Animation>(new Animation{
    .name = strActivate,
    .duration = 4,
    .frames = {
      makeFrame({ 0.5f, 0.f, 0.f, 1.f }),
      makeFrame({ 1.f, 0.6f, 0.6f, 1.f })
    }
  });

  m_animActivate = sysAnimation.addAnimation(std::move(animActivate));
}

EntityId MenuSystemImpl::root() const
{
  return m_root;
}

void MenuSystemImpl::showPauseMenu()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysUi = m_ecs.system<SysUi>();

  sysSpatial.setEnabled(m_mainMenu.entityId, false);
  sysSpatial.setEnabled(m_pauseMenuId, true);
  sysSpatial.setEnabled(m_pauseMenuMain.entityId, true);
  sysSpatial.setEnabled(m_pauseMenuSettingsId, false);

  sysUi.setActiveGroup(m_pauseMenuMain.itemGroupId, m_pauseMenuMain.firstItemId);
}

void MenuSystemImpl::showMainMenu()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysUi = m_ecs.system<SysUi>();

  sysSpatial.setEnabled(m_pauseMenuId, false);
  sysSpatial.setEnabled(m_mainMenu.entityId, true);

  sysUi.setActiveGroup(m_mainMenu.itemGroupId, m_mainMenu.firstItemId);
}

void MenuSystemImpl::constructRoot()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();

  m_root = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(m_root, spatial);
}

void MenuSystemImpl::constructBackdrop()
{
  auto& sysRender = m_ecs.system<SysRender>();
  auto& sysSpatial = m_ecs.system<SysSpatial>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  Vec2f size{ GRID_CELL_W * GRID_W, (5.f + GRID_H) * GRID_CELL_H };
  Vec2f pos{ 0.f, 0.f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_root,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .scissor = MAIN_SCISSOR,
    .textureRect = Rectf{
      .x = pxToUvX(704.f),
      .y = pxToUvY(4.f, 252.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(252.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Backdrop)
  };

  sysRender.addEntity(id, render);
}

void MenuSystemImpl::constructFlare()
{
  auto& sysRender = m_ecs.system<SysRender>();
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysAnimation = m_ecs.system<SysAnimation>();

  auto animRotate = std::unique_ptr<Animation>(new Animation{
    .name = hashString("rotate"),
    .duration = TICKS_PER_SECOND * 650,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .rotation = 360.f,
        .pivot = Vec2f{ 0.5f, 0.5f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      }
    }
  });

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  Vec2f size{ 1.5f, 1.5f };
  Vec2f pos{ 0.145f, -0.05f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = m_root,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .scissor = MAIN_SCISSOR,
    .textureRect = Rectf{
      .x = pxToUvX(704.f),
      .y = pxToUvY(256.f, 256.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(256.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Flare)
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, AnimationData{
    .animations = {
      sysAnimation.addAnimation(std::move(animRotate))
    }
  });

  sysAnimation.playAnimation(id, hashString("rotate"), true);
}

EntityId MenuSystemImpl::newMenuItemId()
{
  return m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite, CUi
  >();
}

void MenuSystemImpl::constructMenuItemBase(EntityId id, EntityId parentId, const Sprite& sprite)
{
  auto& sysRender = m_ecs.system<SysRender>();
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysAnimation = m_ecs.system<SysAnimation>();

  SpatialData spatial{
    .transform = spriteTransform(sprite.pos, sprite.size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .scissor = MAIN_SCISSOR,
    .textureRect = sprite.texRect,
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem)
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, AnimationData{
    .animations = { m_animIdle, m_animIdleFocused, m_animPrime, m_animActivate }
  });
  sysAnimation.playAnimation(id, strIdle, true);
  sysAnimation.seek(id, fge::randomInt());
}

void MenuSystemImpl::constructMenuItem(EntityId id, EntityId parentId, SysUi::GroupId groupId,
  const Sprite& sprite, const ItemSlots& slots)
{
  auto& sysAnimation = m_ecs.system<SysAnimation>();
  auto& sysUi = m_ecs.system<SysUi>();

  constructMenuItemBase(id, parentId, sprite);

  UiData ui{};
  ui.group = groupId;
  ui.topSlot = slots.top;
  ui.bottomSlot = slots.bottom;
  ui.inputFilter = {
    MouseButton::Left, KeyboardKey::Enter, KeyboardKey::Space, KeyboardKey::Escape
  };
  ui.onInputEnd = [this, &sysAnimation, id](const UserInput& input) {
    bool escape = std::holds_alternative<KeyboardKey>(input) &&
      std::get<KeyboardKey>(input) == KeyboardKey::Escape;

    if (!escape) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strActivate, [this, id]() {
        m_eventSystem.raiseEvent(EMenuItemActivate{id});
      });
    }
    else {
      m_eventSystem.raiseEvent(ESubmenuExit{});
    }
  };
  ui.onGainFocus = [&sysAnimation, id]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strIdleFocused, true);
  };
  ui.onLoseFocus = [&sysAnimation, id]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strIdle, true);
  };
  ui.onInputCancel = [&sysAnimation, id]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strIdleFocused, true);
  };
  ui.onInputBegin = [&sysAnimation, id](const UserInput& input) {
    bool escape = std::holds_alternative<KeyboardKey>(input) &&
      std::get<KeyboardKey>(input) == KeyboardKey::Escape;

    if (!escape) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strPrime);
    }
  };

  sysUi.addEntity(id, ui);
}

void MenuSystemImpl::constructSelector(EntityId id, EntityId parentId, SysUi::GroupId groupId,
  const Sprite& sprite, const ItemSlots& slots, const SelectorFunctions& functions)
{
  auto& sysAnimation = m_ecs.system<SysAnimation>();
  auto& sysUi = m_ecs.system<SysUi>();

  constructMenuItemBase(id, parentId, sprite);

  auto leftBtnId = newMenuItemId();
  auto rightBtnId = newMenuItemId();

  Sprite leftBtnSprite{
    .pos{ 0.4f, sprite.pos[1] },
    .size{ 0.075f, 0.05625f },
    .texRect{
      .x = pxToUvX(192.f),
      .y = pxToUvY(0.f, 32.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItemBase(leftBtnId, parentId, leftBtnSprite);

  Sprite rightBtnSprite{
    .pos{ 0.5f, sprite.pos[1] },
    .size{ 0.075f, 0.05625f },
    .texRect{
      .x = pxToUvX(224.f),
      .y = pxToUvY(0.f, 32.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItemBase(rightBtnId, parentId, rightBtnSprite);

  auto focusAll = [&sysAnimation, id, leftBtnId, rightBtnId]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtnId);
    sysAnimation.stopAnimation(rightBtnId);
    sysAnimation.queueAnimation(id, strIdleFocused, true);
    sysAnimation.queueAnimation(leftBtnId, strIdleFocused, true);
    sysAnimation.queueAnimation(rightBtnId, strIdleFocused, true);
  };

  auto unfocusAll = [&sysAnimation, id, leftBtnId, rightBtnId]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtnId);
    sysAnimation.stopAnimation(rightBtnId);
    sysAnimation.queueAnimation(id, strIdle, true);
    sysAnimation.queueAnimation(leftBtnId, strIdle, true);
    sysAnimation.queueAnimation(rightBtnId, strIdle, true);
  };

  std::set<UserInput> filter{
    MouseButton::Left, KeyboardKey::Enter, KeyboardKey::Space, KeyboardKey::Left,
    KeyboardKey::Right, KeyboardKey::Escape
  };

  UiData ui{};
  ui.group = groupId;
  ui.inputFilter = filter;
  ui.topSlot = slots.top;
  ui.bottomSlot = slots.bottom;
  ui.onGainFocus = focusAll;
  ui.onLoseFocus = unfocusAll;
  ui.onInputCancel = [&sysUi, focusAll, leftBtnId, rightBtnId]() {
    sysUi.sendInputCancel(leftBtnId);
    sysUi.sendInputCancel(rightBtnId);
    focusAll();
  };
  ui.onInputBegin = [&sysUi, leftBtnId, rightBtnId](const UserInput& input) {
    if (std::holds_alternative<KeyboardKey>(input)) {
      if (std::get<KeyboardKey>(input) == KeyboardKey::Left) {
        sysUi.sendInputBegin(leftBtnId, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Right) {
        sysUi.sendInputBegin(rightBtnId, input);
      }
    }
  };
  ui.onInputEnd = [this, &sysUi, leftBtnId, rightBtnId](const UserInput& input) {
    if (std::holds_alternative<KeyboardKey>(input)) {
      if (std::get<KeyboardKey>(input) == KeyboardKey::Left) {
        sysUi.sendInputEnd(leftBtnId, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Right) {
        sysUi.sendInputEnd(rightBtnId, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Escape) {
        m_eventSystem.raiseEvent(ESubmenuExit{});
      }
    }
  };

  sysUi.addEntity(id, ui);

  auto makeBtnUiComp =
    [&sysAnimation, &sysUi, &filter, id, groupId, unfocusAll]
    (EntityId btnId, const std::function<void()>& onBtnDown, const std::function<void()>& onBtnUp) {

    auto onBtnUpWrap = [&sysUi, onBtnUp, id]() {
      sysUi.sendFocus(id);
      onBtnUp();
    };

    UiData btnUi{};
    btnUi.group = groupId;
    btnUi.inputFilter = filter;
    btnUi.onGainFocus = [&sysUi, id]() {
      sysUi.sendFocus(id);
    };
    btnUi.onLoseFocus = unfocusAll;
    btnUi.onInputCancel = onBtnUpWrap;
    btnUi.onInputBegin = [&sysAnimation, btnId, onBtnDown](const UserInput&) {
      sysAnimation.stopAnimation(btnId);
      sysAnimation.queueAnimation(btnId, strPrime);
      onBtnDown();
    };
    btnUi.onInputEnd = [&sysAnimation, btnId, onBtnUpWrap](const UserInput&) {
      sysAnimation.stopAnimation(btnId);
      sysAnimation.playAnimation(btnId, strActivate, onBtnUpWrap);
    };

    sysUi.addEntity(btnId, btnUi);
  };

  makeBtnUiComp(leftBtnId, functions.leftBtnDown, functions.leftBtnUp);
  makeBtnUiComp(rightBtnId, functions.rightBtnDown, functions.rightBtnUp);
}

EntityId MenuSystemImpl::constructQuad(EntityId parentId, const Vec2f& pos, const Vec2f& size,
  const Vec4f& colour)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CQuad
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  QuadData render{
    .scissor = MAIN_SCISSOR,
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = colour,
    .radius = 0.f
  };

  sysRender.addEntity(id, render);

  return id;
}

Slider MenuSystemImpl::constructSlider(EntityId parentId, const Vec2f& pos,
  float initialValue)
{
  const float gap = 0.005f;
  const Vec2f size{ 0.03f, 0.3f };
  const Vec2f sliderSize = size - Vec2f{ gap, gap } * 2.f;
  const Vec4f colour{ 0.f, 0.f, 0.f, 0.7f };
  const float lineThickness = 0.002f;

  // Slider
  auto id = constructQuad(parentId, pos + Vec2f{ gap, gap },
    { sliderSize[0], sliderSize[1] * initialValue }, colour);

  // Bottom line
  constructQuad(parentId, pos, { size[0], lineThickness }, colour);

  // Top line
  constructQuad(parentId, { pos[0], pos[1] + size[1] - lineThickness }, { size[0], lineThickness },
    colour);

  // Left line
  constructQuad(parentId, pos, { lineThickness, size[1] }, colour);

  // Right line
  constructQuad(parentId, { pos[0] + size[0] - lineThickness, pos[1] }, { lineThickness, size[1] },
    colour);

  return Slider{
    .entityId = id,
    .height = sliderSize[1]
  };
}

Menu MenuSystemImpl::constructSettingsSubmenu(EntityId parentId, const Menu& prevMenu)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysUi = m_ecs.system<SysUi>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = parentId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  auto musicVolumeId = newMenuItemId();
  auto sfxVolumeId = newMenuItemId();
  auto returnId = newMenuItemId();

  auto groupId = SysUi::nextGroupId();

  Sprite musicVolumeSprite{
    .pos{ 0.02f, 0.21f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(96.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  SelectorFunctions musicVolumeFunctions{
    .leftBtnDown = [this]() { m_musicVolumeDelta = -0.01f; },
    .leftBtnUp = [this]() { m_musicVolumeDelta = 0.f; },
    .rightBtnDown = [this]() { m_musicVolumeDelta = 0.01f; },
    .rightBtnUp = [this]() { m_musicVolumeDelta = 0.f; }
  };

  constructSelector(musicVolumeId, id, groupId, musicVolumeSprite, { returnId, sfxVolumeId },
    musicVolumeFunctions);

  Sprite sfxVolumeSprite{
    .pos{ 0.02f, 0.11f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(128.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  SelectorFunctions sfxVolumeFunctions{
    .leftBtnDown = [this]() { m_sfxVolumeDelta = -0.01f; },
    .leftBtnUp = [this]() { m_sfxVolumeDelta = 0.f; },
    .rightBtnDown = [this]() { m_sfxVolumeDelta = 0.01f; },
    .rightBtnUp = [this]() { m_sfxVolumeDelta = 0.f; }
  };

  constructSelector(sfxVolumeId, id, groupId, sfxVolumeSprite, { musicVolumeId, returnId },
    sfxVolumeFunctions);

  auto musicIconId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  SpatialData musicIconSpatial{
    .transform = spriteTransform({ 0.75f, 0.55f }, { 0.05f, 0.1f }),
    .parent = id,
    .enabled = true
  };

  sysSpatial.addEntity(musicIconId, musicIconSpatial);

  SpriteData musicIconRender{
    .scissor = MAIN_SCISSOR,
    .textureRect = Rectf{
      .x = pxToUvX(96.f),
      .y = pxToUvY(448.f, 64.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(64.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem)
  };

  sysRender.addEntity(musicIconId, musicIconRender);

  auto sfxIconId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  SpatialData sfxIconSpatial{
    .transform = spriteTransform({ 0.975f, 0.55f }, { 0.1f, 0.066f }),
    .parent = id,
    .enabled = true
  };

  sysSpatial.addEntity(sfxIconId, sfxIconSpatial);

  SpriteData sfxIconRender{
    .scissor = MAIN_SCISSOR,
    .textureRect = Rectf{
      .x = pxToUvX(0.f),
      .y = pxToUvY(448.f, 64.f),
      .w = pxToUvW(96.f),
      .h = pxToUvH(64.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem)
  };

  sysRender.addEntity(sfxIconId, sfxIconRender);

  if (parentId == m_mainMenu.entityId) {
    m_musicVolumeSlider1 = constructSlider(id, { 0.85f, 0.55f }, m_musicVolume);
    m_sfxVolumeSlider1 = constructSlider(id, { 0.91f, 0.55f }, m_sfxVolume);
  }
  else {
    m_musicVolumeSlider2 = constructSlider(id, { 0.85f, 0.55f }, m_musicVolume);
    m_sfxVolumeSlider2 = constructSlider(id, { 0.91f, 0.55f }, m_sfxVolume);
  }

  Sprite returnSprite{
    .pos{ 0.02f, 0.01f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(160.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(returnId, id, groupId, returnSprite, { sfxVolumeId, musicVolumeId });

  auto behaviour = fge::createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit },
    [id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

    if ((e.name == g_strMenuItemActivate
      && dynamic_cast<const EMenuItemActivate&>(e).entityId == returnId) ||
      e.name == g_strSubmenuExit) {

      sysSpatial.setEnabled(id, false);
      sysSpatial.setEnabled(prevMenu.entityId, true);
      sysUi.setActiveGroup(prevMenu.itemGroupId);
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId, musicVolumeId };
}

void MenuSystemImpl::constructMainMenu(bool hasQuitButton)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysUi = m_ecs.system<SysUi>();

  m_mainMenu.entityId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData mainMenuRootSpatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_mainMenu.entityId, mainMenuRootSpatial);

  auto mainMenuMainId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData mainMenuMainSpatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = true
  };

  sysSpatial.addEntity(mainMenuMainId, mainMenuMainSpatial);

  m_mainMenu.itemGroupId = SysUi::nextGroupId();

  m_startGameBtnId = newMenuItemId();
  auto optionsId = newMenuItemId();
  auto settingsId = newMenuItemId();
  auto creditsId = newMenuItemId();
  m_quitGameBtnId = hasQuitButton ? newMenuItemId() : NULL_ENTITY;

  m_mainMenu.firstItemId = m_startGameBtnId;

  float y = hasQuitButton ? 0.41f : 0.31f;

  Sprite startSprite{
    .pos{ 0.02f, y },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(0.f, 32.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(m_startGameBtnId, mainMenuMainId, m_mainMenu.itemGroupId, startSprite,
    { hasQuitButton ? m_quitGameBtnId : creditsId, optionsId });

  y -= 0.1f;

  Sprite optionsSprite{
    .pos{ 0.02f, y },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(320.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(optionsId, mainMenuMainId, m_mainMenu.itemGroupId, optionsSprite,
    { m_startGameBtnId, settingsId });

  y -= 0.1f;

  Sprite settingsSprite{
    .pos{ 0.02f, y },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(32.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(settingsId, mainMenuMainId, m_mainMenu.itemGroupId, settingsSprite,
    { optionsId, creditsId });

  y -= 0.1f;

  Sprite creditsSprite{
    .pos{ 0.02f, y },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(64.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(creditsId, mainMenuMainId, m_mainMenu.itemGroupId, creditsSprite,
    { settingsId, hasQuitButton ? m_quitGameBtnId : m_startGameBtnId });

  y -= 0.1f;

  if (hasQuitButton) {
    Sprite quitSprite{
      .pos{ 0.02f, y },
      .size{ 0.4f, 0.05625f },
      .texRect{
        .x = pxToUvX(0.f),
        .y = pxToUvY(192.f, 32.f),
        .w = pxToUvW(256.f),
        .h = pxToUvH(32.f)
      }
    };

    constructMenuItem(m_quitGameBtnId, mainMenuMainId, m_mainMenu.itemGroupId, quitSprite,
      { creditsId, m_startGameBtnId });
  }

  auto mainMenuSettings = constructSettingsSubmenu(m_mainMenu.entityId,
    { mainMenuMainId, m_mainMenu.itemGroupId });
  auto mainMenuOptions = constructGameOptionsSubmenu({ mainMenuMainId, m_mainMenu.itemGroupId });
  auto mainMenuCredits = constructCreditsSubmenu({ mainMenuMainId, m_mainMenu.itemGroupId });

  auto behaviour = fge::createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, &sysSpatial, &sysUi, settingsId, mainMenuMainId, mainMenuSettings, optionsId,
      mainMenuOptions, creditsId, mainMenuCredits](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == settingsId) {
        sysSpatial.setEnabled(mainMenuMainId, false);
        sysSpatial.setEnabled(mainMenuSettings.entityId, true);
        sysUi.setActiveGroup(mainMenuSettings.itemGroupId, mainMenuSettings.firstItemId);
      }
      else if (event.entityId == optionsId) {
        sysSpatial.setEnabled(mainMenuMainId, false);
        sysSpatial.setEnabled(mainMenuOptions.entityId, true);
        sysUi.setActiveGroup(mainMenuOptions.itemGroupId, mainMenuOptions.firstItemId);
        setDifficulty(m_difficultyLevel); // Refresh counters
      }
      else if (event.entityId == creditsId) {
        sysSpatial.setEnabled(mainMenuMainId, false);
        sysSpatial.setEnabled(mainMenuCredits.entityId, true);
        sysUi.setActiveGroup(mainMenuCredits.itemGroupId, mainMenuCredits.firstItemId);
      }
    }
  });

  sysBehaviour.addBehaviour(m_mainMenu.entityId, std::move(behaviour));
}

EntityId MenuSystemImpl::constructTextItem(EntityId parentId, const Vec2f& pos,
  const Vec2f& charSize, const std::string& text, const Vec4f& colour)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, charSize),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  TextData render{
    .scissor = MAIN_SCISSOR,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = text,
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = colour
  };

  sysRender.addEntity(id, render);

  return id;
}

void MenuSystemImpl::addFadeInAnimForEntity(EntityId entityId)
{
  static const HashedString strFadeIn = hashString("fade_in");

  // Fade-in animations for each colour
  static std::map<Vec4f, AnimationId> animations;

  auto& sysAnimation = m_ecs.system<SysAnimation>();

  auto makeFrame = [](const Vec4f& colour) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = std::nullopt,
      .colour = colour
    };
  };

  auto alpha = [](const Vec4f& colour, float value) {
    auto copy = colour;
    copy[3] = value;
    return copy;
  };

  const Vec4f& colour = m_ecs.componentStore().component<CRender>(entityId).colour;

  AnimationId animFadeInId = 0;
  if (animations.contains(colour)) {
    animFadeInId = animations.at(colour);
  }
  else {
    auto anim = std::unique_ptr<Animation>(new Animation{
      .name = strFadeIn,
      .duration = 60,
      .frames = {
        makeFrame(alpha(colour, 0.1f)),
        makeFrame(alpha(colour, 0.2f)),
        makeFrame(alpha(colour, 0.3f)),
        makeFrame(alpha(colour, 0.4f)),
        makeFrame(alpha(colour, 0.5f)),
        makeFrame(alpha(colour, 0.6f)),
        makeFrame(alpha(colour, 0.7f)),
        makeFrame(alpha(colour, 0.8f)),
        makeFrame(alpha(colour, 0.9f)),
        makeFrame(alpha(colour, 1.f)),
      }
    });
    animFadeInId = sysAnimation.addAnimation(std::move(anim));
  }

  sysAnimation.addEntity(entityId, AnimationData{
    .animations{ animFadeInId }
  });
}

Menu MenuSystemImpl::constructCreditsSubmenu(const Menu& prevMenu)
{
  static const HashedString strFadeIn = hashString("fade_in");

  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysUi = m_ecs.system<SysUi>();
  auto& sysAnimation = m_ecs.system<SysAnimation>();
  auto& sysRender = m_ecs.system<SysRender>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  auto logoId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  float h = 0.26f;
  float w = 1.f * h;

  SpatialData logoSpatial{
    .transform = spriteTransform({ 0.765f, 0.55f }, { w, h }),
    .parent = id,
    .enabled = true
  };

  sysSpatial.addEntity(logoId, logoSpatial);

  SpriteData logoRender{
    .scissor = MAIN_SCISSOR,
    .textureRect{
      .x = pxToUvX(512.f),
      .y = pxToUvY(384.f, 128.f),
      .w = pxToUvW(128.f),
      .h = pxToUvH(128.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = Vec4f{ 0.f, 0.f, 0.f, 0.f }
  };

  sysRender.addEntity(logoId, logoRender);

  std::string txt1 = "Code:  Rob Jinman";
  std::string txt2 = "Music: Jack Normal";
  std::string txt3 = "Sprites: http://untamed.wild-refuge.net";
  std::string txt4 = getVersionString();

  std::array<EntityId, 5> items{
    constructTextItem(id, { 0.7f, 0.42f }, { 0.022f, 0.044f }, txt1, { 1.f, 1.f, 1.f, 0.f }),
    constructTextItem(id, { 0.7f, 0.37f }, { 0.022f, 0.044f }, txt2, { 1.f, 1.f, 1.f, 0.f }),
    constructTextItem(id, { 0.5f, 0.02f }, { 0.02f, 0.04f }, txt3, { 0.5f, 0.5f, 0.5f, 0.f }),
    constructTextItem(id, { 0.014f, 0.96f }, { 0.0175f, 0.035f }, txt4, { 0.7f, 0.7f, 0.7f, 0.f }),
    logoId
  };

  addFadeInAnimForEntity(items[0]);
  addFadeInAnimForEntity(items[1]);
  addFadeInAnimForEntity(items[2]);
  addFadeInAnimForEntity(items[3]);
  addFadeInAnimForEntity(items[4]);

  auto groupId = SysUi::nextGroupId();

  auto returnId = newMenuItemId();

  Sprite returnSprite{
    .pos{ 0.02f, 0.01f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(160.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(returnId, id, groupId, returnSprite, { NULL_ENTITY, NULL_ENTITY });

  auto behaviour = fge::createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit, fge::g_strEntityEnable },
    [id, prevMenu, returnId, items, &sysAnimation, &sysSpatial, &sysRender, &sysUi]
    (const Event& e) {

    if ((e.name == g_strMenuItemActivate &&
      dynamic_cast<const EMenuItemActivate&>(e).entityId == returnId) ||
      e.name == g_strSubmenuExit) {

      for (auto textItem : items) {
        sysAnimation.stopAnimation(textItem);
        sysRender.setColour(textItem, { 0.f, 0.f, 0.f, 0.f });
      }

      sysSpatial.setEnabled(id, false);
      sysSpatial.setEnabled(prevMenu.entityId, true);
      sysUi.setActiveGroup(prevMenu.itemGroupId);
    }
    else if (e.name == fge::g_strEntityEnable) {
      auto& event = dynamic_cast<const fge::EEntityEnable&>(e);
      if (event.entityId == id) {
        for (auto textItem : items) {
          sysAnimation.playAnimation(textItem, strFadeIn);
        }
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId, returnId };
}

EntityId MenuSystemImpl::constructGameOptionCounter(EntityId parentId, const Vec2f& pos,
  const std::string& text, uint32_t value)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysRender = m_ecs.system<SysRender>();

  Vec2f charSize{ 0.022f, 0.044f };
  Vec4f colour{ 0.f, 0.f, 0.f, 1.f };

  constructTextItem(parentId, pos, charSize, text, colour);

  auto counterId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite, CDynamicText
  >();

  float margin = 0.35f;

  SpatialData spatial{
    .transform = spriteTransform(Vec2f{ pos[0] + margin, pos[1] }, charSize),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(counterId, spatial);

  DynamicTextData render{
    .scissor = MAIN_SCISSOR,
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .text = value == 0 ? "---" : std::to_string(value),
    .maxLength = 6,
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = Vec4f{ 0.f, 0.f, 0.f, 1.f }
  };

  sysRender.addEntity(counterId, render);

  return counterId;
}

Menu MenuSystemImpl::constructGameOptionsSubmenu(const Menu& prevMenu)
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysUi = m_ecs.system<SysUi>();

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  auto groupId = SysUi::nextGroupId();

  auto difficultyId = newMenuItemId();
  auto returnId = newMenuItemId();

  Sprite difficultySprite{
    .pos{ 0.02f, 0.11f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(288.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  SelectorFunctions difficultyFunctions;
  difficultyFunctions.leftBtnUp = [this]() {
    if (m_difficultyLevel > 0) {
      setDifficulty(m_difficultyLevel - 1);
    }
  };
  difficultyFunctions.rightBtnUp = [this]() {
    if (m_difficultyLevel + 1 <= MAX_DIFFICULTY_LEVEL) {
      setDifficulty(m_difficultyLevel + 1);
    }
  };

  constructSelector(difficultyId, id, groupId, difficultySprite, { returnId, returnId },
    difficultyFunctions);

  float value = static_cast<float>(m_difficultyLevel) / MAX_DIFFICULTY_LEVEL;
  m_gameDifficultySlider = constructSlider(id, { 0.65f, 0.55f }, value);

  GameOptions options = m_options.getOptionsForLevel(0);
  uint32_t totalGold = options.coins + 5 * options.nuggets;
  m_gameOptionCounters = {
    .minesId = constructGameOptionCounter(id, { 0.75f, 0.85f }, "Land mines:", options.mines),
    .totalGoldId = constructGameOptionCounter(id, { 0.75f, 0.80f }, "Total gold:", totalGold),
    .sticksId = constructGameOptionCounter(id, { 0.75f, 0.75f }, "Sticks:", options.sticks),
    .wanderersId = constructGameOptionCounter(id, { 0.75f, 0.70f }, "Wanderers:",
      options.wanderers),
    .goldRequiredId = constructGameOptionCounter(id, { 0.75f, 0.65f }, "Gold required:",
      options.goldRequired),
    .timeAvailableId = constructGameOptionCounter(id, { 0.75f, 0.60f }, "Time available:",
      options.timeAvailable),
    .bestTimeId = constructGameOptionCounter(id, { 0.75f, 0.50f }, "Best time:", 0)
  };

  Sprite returnSprite{
    .pos{ 0.02f, 0.01f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(160.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(returnId, id, groupId, returnSprite, { difficultyId, difficultyId });

  auto behaviour = fge::createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit },
    [id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

    if ((e.name == g_strMenuItemActivate &&
      dynamic_cast<const EMenuItemActivate&>(e).entityId == returnId) ||
      e.name == g_strSubmenuExit) {

      sysSpatial.setEnabled(id, false);
      sysSpatial.setEnabled(prevMenu.entityId, true);
      sysUi.setActiveGroup(prevMenu.itemGroupId);
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId, difficultyId };
}

void MenuSystemImpl::constructPauseMenu()
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& sysBehaviour = m_ecs.system<SysBehaviour>();
  auto& sysUi = m_ecs.system<SysUi>();

  m_pauseMenuId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuRootSpatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_pauseMenuId, pauseMenuRootSpatial);

  m_pauseMenuMain.entityId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuMainSpatial{
    .transform = identityMatrix<float, 4>(),
    .parent = m_pauseMenuId,
    .enabled = true
  };

  sysSpatial.addEntity(m_pauseMenuMain.entityId, pauseMenuMainSpatial);

  m_pauseMenuMain.itemGroupId = SysUi::nextGroupId();

  m_resumeBtnId = newMenuItemId();
  auto settingsId = newMenuItemId();
  m_quitToMainBtnId = newMenuItemId();

  m_pauseMenuMain.firstItemId = m_resumeBtnId;

  Sprite resumeSprite{
    .pos{ 0.02f, 0.21f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(224.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(m_resumeBtnId, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    resumeSprite, { m_quitToMainBtnId, settingsId });

  Sprite settingsSprite{
    .pos{ 0.02f, 0.11f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(32.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(settingsId, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    settingsSprite, { m_resumeBtnId, m_quitToMainBtnId });

  Sprite quitSprite{
    .pos{ 0.02f, 0.01f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(256.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(m_quitToMainBtnId, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    quitSprite, { settingsId, m_resumeBtnId });

  auto pauseMenuSettings = constructSettingsSubmenu(m_pauseMenuId,
    { m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId });
  m_pauseMenuSettingsId = pauseMenuSettings.entityId;

  auto behaviour = fge::createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, &sysSpatial, &sysUi, settingsId, pauseMenuSettings](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == settingsId) {
        sysSpatial.setEnabled(m_pauseMenuMain.entityId, false);
        sysSpatial.setEnabled(m_pauseMenuSettingsId, true);
        sysUi.setActiveGroup(pauseMenuSettings.itemGroupId, pauseMenuSettings.firstItemId);
      }
    }
  });

  sysBehaviour.addBehaviour(m_pauseMenuId, std::move(behaviour));
}

}

MenuSystemPtr createMenuSystem(Ecs& ecs, EventSystem& eventSystem,
  const GameOptionsManager& options, fge::Logger& logger, bool hasQuitButton)
{
  return std::make_unique<MenuSystemImpl>(ecs, eventSystem, options, logger, hasQuitButton);
}

#include "menu_system.hpp"
#include "ecs.hpp"
#include "event_system.hpp"
#include "logger.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "sys_behaviour.hpp"
#include "sys_animation.hpp"
#include "sys_ui.hpp"
#include "systems.hpp"
#include "b_generic.hpp"
#include "input.hpp"
#include "game_options.hpp"
#include <cstring>

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
  return translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
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
  float_t height;
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
    MenuSystemImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

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
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    Logger& m_logger;
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
    void constructMainMenu();
    Menu constructSettingsSubmenu(EntityId parentId, const Menu& prevMenu);
    Menu constructCreditsSubmenu(const Menu& prevMenu);
    Menu constructGameOptionsSubmenu(const Menu& prevMenu);
    void createAnimations();
    EntityId newMenuItemId();
    void setDifficulty(int level);
    Slider constructSlider(EntityId parentId, const Vec2f& pos, float_t initialValue);
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
    EntityId constructFadeInText(EntityId parentId, const Vec2f& pos, const Vec2f& charSize,
      const std::string& text, const Vec4f& colour);
    void updateSlider(const Slider& slider, float_t value);
};

MenuSystemImpl::MenuSystemImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_logger(logger)
{
  createAnimations();

  constructRoot();
  constructBackdrop();
  constructFlare();
  constructPauseMenu();
  constructMainMenu();
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
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  m_difficultyLevel = level;

  float_t value = static_cast<float_t>(level) / MAX_DIFFICULTY_LEVEL;
  updateSlider(m_gameDifficultySlider, value);

  GameOptions options = getOptionsForLevel(level);

  uint32_t totalGold = options.coins + 5 * options.nuggets;

  const auto& counters = m_gameOptionCounters;
  sysRender.updateDynamicText(counters.minesId, std::to_string(options.mines));
  sysRender.updateDynamicText(counters.totalGoldId, std::to_string(totalGold));
  sysRender.updateDynamicText(counters.sticksId, std::to_string(options.sticks));
  sysRender.updateDynamicText(counters.wanderersId, std::to_string(options.wanderers));
  sysRender.updateDynamicText(counters.goldRequiredId, std::to_string(options.goldRequired));
  sysRender.updateDynamicText(counters.timeAvailableId, std::to_string(options.timeAvailable));
  sysRender.updateDynamicText(counters.bestTimeId, "---"); // TODO
}

void MenuSystemImpl::updateSlider(const Slider& slider, float_t value)
{
  auto& t = m_ecs.componentStore().component<CLocalTransform>(slider.entityId);
  t.transform.set(1, 1, value * slider.height);
}

void MenuSystemImpl::update()
{
  if (m_musicVolumeDelta != 0.f) {
    m_musicVolume = clip(m_musicVolume + m_musicVolumeDelta, 0.f, 1.f);
    updateSlider(m_musicVolumeSlider1, m_musicVolume);
    updateSlider(m_musicVolumeSlider2, m_musicVolume);
  }

  if (m_sfxVolumeDelta != 0.f) {
    m_sfxVolume = clip(m_sfxVolume + m_sfxVolumeDelta, 0.f, 1.f);
    updateSlider(m_sfxVolumeSlider1, m_sfxVolume);
    updateSlider(m_sfxVolumeSlider2, m_sfxVolume);
  }
}

void MenuSystemImpl::createAnimations()
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  sysSpatial.setEnabled(m_mainMenu.entityId, false);
  sysSpatial.setEnabled(m_pauseMenuId, true);
  sysSpatial.setEnabled(m_pauseMenuMain.entityId, true);
  sysSpatial.setEnabled(m_pauseMenuSettingsId, false);

  sysUi.setActiveGroup(m_pauseMenuMain.itemGroupId, m_pauseMenuMain.firstItemId);
}

void MenuSystemImpl::showMainMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  sysSpatial.setEnabled(m_pauseMenuId, false);
  sysSpatial.setEnabled(m_mainMenu.entityId, true);

  sysUi.setActiveGroup(m_mainMenu.itemGroupId, m_mainMenu.firstItemId);
}

void MenuSystemImpl::constructRoot()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  m_root = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(m_root, spatial);
}

void MenuSystemImpl::constructBackdrop()
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

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
    .textureRect = Rectf{
      .x = pxToUvX(704.f),
      .y = pxToUvY(0.f, 256.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(256.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Backdrop)
  };

  sysRender.addEntity(id, render);
}

void MenuSystemImpl::constructFlare()
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto animRotate = std::unique_ptr<Animation>(new Animation{
    .name = hashString("rotate"),
    .duration = TICKS_PER_SECOND * 450,
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
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  SpatialData spatial{
    .transform = spriteTransform(sprite.pos, sprite.size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = sprite.texRect,
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem)
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, AnimationData{
    .animations = { m_animIdle, m_animIdleFocused, m_animPrime, m_animActivate }
  });
  sysAnimation.playAnimation(id, strIdle, true);
  sysAnimation.seek(id, randomInt());
}

void MenuSystemImpl::constructMenuItem(EntityId id, EntityId parentId, SysUi::GroupId groupId,
  const Sprite& sprite, const ItemSlots& slots)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

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
        m_eventSystem.queueEvent(std::make_unique<EMenuItemActivate>(id));
      });
    }
    else {
      m_eventSystem.queueEvent(std::make_unique<ESubmenuExit>());
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
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

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

  auto focusAll = [&sysAnimation, &sysUi, id, leftBtnId, rightBtnId]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtnId);
    sysAnimation.stopAnimation(rightBtnId);
    sysAnimation.playAnimation(id, strIdleFocused, true);
    sysAnimation.playAnimation(leftBtnId, strIdleFocused, true);
    sysAnimation.playAnimation(rightBtnId, strIdleFocused, true);
  };

  auto unfocusAll = [&sysAnimation, id, leftBtnId, rightBtnId]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtnId);
    sysAnimation.stopAnimation(rightBtnId);
    sysAnimation.playAnimation(id, strIdle, true);
    sysAnimation.playAnimation(leftBtnId, strIdle, true);
    sysAnimation.playAnimation(rightBtnId, strIdle, true);
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
        m_eventSystem.queueEvent(std::make_unique<ESubmenuExit>());
      }
    }
  };

  sysUi.addEntity(id, ui);

  auto makeBtnUiComp =
    [this, &sysAnimation, &sysUi, &filter, id, groupId, unfocusAll]
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
      sysAnimation.playAnimation(btnId, strPrime);
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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  QuadData render{
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = colour
  };

  sysRender.addEntity(id, render);

  return id;
}

Slider MenuSystemImpl::constructSlider(EntityId parentId, const Vec2f& pos,
  float_t initialValue)
{
  const float_t gap = 0.005f;
  const Vec2f size{ 0.03f, 0.3f };
  const Vec2f sliderSize = size - Vec2f{ gap, gap } * 2.f;
  const Vec4f colour{ 0.f, 0.f, 0.f, 0.7f };
  const float_t lineThickness = 0.002f;

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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
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

  auto behaviour = createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit },
    [this, id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

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

void MenuSystemImpl::constructMainMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  m_mainMenu.entityId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData mainMenuRootSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_mainMenu.entityId, mainMenuRootSpatial);

  auto mainMenuMainId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData mainMenuMainSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = true
  };

  sysSpatial.addEntity(mainMenuMainId, mainMenuMainSpatial);

  m_mainMenu.itemGroupId = SysUi::nextGroupId();

  m_startGameBtnId = newMenuItemId();
  auto optionsId = newMenuItemId();
  auto settingsId = newMenuItemId();
  auto creditsId = newMenuItemId();
  m_quitGameBtnId = newMenuItemId();

  m_mainMenu.firstItemId = m_startGameBtnId;

  Sprite startSprite{
    .pos{ 0.02f, 0.41f },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(0.f, 32.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(m_startGameBtnId, mainMenuMainId, m_mainMenu.itemGroupId, startSprite,
    { m_quitGameBtnId, optionsId });

  Sprite optionsSprite{
    .pos{ 0.02f, 0.31f },
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

  Sprite settingsSprite{
    .pos{ 0.02f, 0.21f },
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

  Sprite creditsSprite{
    .pos{ 0.02f, 0.11f },
    .size{ 0.4f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(64.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(creditsId, mainMenuMainId, m_mainMenu.itemGroupId, creditsSprite,
    { settingsId, m_quitGameBtnId });

  Sprite quitSprite{
    .pos{ 0.02f, 0.01f },
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

  auto mainMenuSettings = constructSettingsSubmenu(m_mainMenu.entityId,
    { mainMenuMainId, m_mainMenu.itemGroupId });
  auto mainMenuOptions = constructGameOptionsSubmenu({ mainMenuMainId, m_mainMenu.itemGroupId });
  auto mainMenuCredits = constructCreditsSubmenu({ mainMenuMainId, m_mainMenu.itemGroupId });

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [=, &sysSpatial, &sysUi](const Event& e) {

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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

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

EntityId MenuSystemImpl::constructFadeInText(EntityId parentId, const Vec2f& pos,
  const Vec2f& charSize, const std::string& text, const Vec4f& colour)
{
  static const HashedString strFadeIn = hashString("fade_in");

  // Fade-in animations for each colour
  static std::map<Vec4f, AnimationId> animations;

  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto makeFrame = [](const Vec4f& colour) {
    return AnimationFrame{
      .pos = Vec2f{ 0.f, 0.f },
      .scale = Vec2f{ 1.f, 1.f },
      .textureRect = std::nullopt,
      .colour = colour
    };
  };

  auto alpha = [](const Vec4f& colour, float_t value) {
    auto copy = colour;
    copy[3] = value;
    return copy;
  };

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

  auto id = constructTextItem(parentId, pos, charSize, text, colour);

  sysAnimation.addEntity(id, AnimationData{
    .animations{ animFadeInId }
  });

  return id;
}

Menu MenuSystemImpl::constructCreditsSubmenu(const Menu& prevMenu)
{
  static const HashedString strFadeIn = hashString("fade_in");

  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  std::string txt1 = "Design & Programming: Rob Jinman";
  std::string txt2 = "Music:                Jack Normal";
  std::string txt3 = "Sprites: http://untamed.wild-refuge.net";
  std::string txt4 = "v2.0"; // TODO

  std::array<EntityId, 4> textItems{
    constructFadeInText(id, { 0.54f, 0.7f }, { 0.022f, 0.044f }, txt1, { 0.f, 0.f, 0.f, 0.f }),
    constructFadeInText(id, { 0.54f, 0.65f }, { 0.022f, 0.044f }, txt2, { 0.f, 0.f, 0.f, 0.f }),
    constructFadeInText(id, { 0.5f, 0.02f }, { 0.02f, 0.04f }, txt3, { 0.5f, 0.5f, 0.5f, 0.f }),
    constructFadeInText(id, { 0.014f, 0.96f }, { 0.0175f, 0.035f }, txt4, { 0.7f, 0.7f, 0.7f, 0.f })
  };

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

  auto behaviour = createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit, g_strEntityEnable },
    [this, id, prevMenu, returnId, textItems, &sysAnimation, &sysSpatial, &sysRender, &sysUi]
    (const Event& e) {

    if ((e.name == g_strMenuItemActivate &&
      dynamic_cast<const EMenuItemActivate&>(e).entityId == returnId) ||
      e.name == g_strSubmenuExit) {

      for (auto textItem : textItems) {
        sysAnimation.stopAnimation(textItem);
        sysRender.setColour(textItem, { 0.f, 0.f, 0.f, 0.f });
      }

      sysSpatial.setEnabled(id, false);
      sysSpatial.setEnabled(prevMenu.entityId, true);
      sysUi.setActiveGroup(prevMenu.itemGroupId);
    }
    else if (e.name == g_strEntityEnable) {
      auto& event = dynamic_cast<const EEntityEnable&>(e);
      if (event.entityId == id) {
        for (auto textItem : textItems) {
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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  Vec2f charSize{ 0.022f, 0.044f };
  Vec4f colour{ 0.f, 0.f, 0.f, 1.f };

  constructTextItem(parentId, pos, charSize, text, colour);

  auto counterId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite, CDynamicText
  >();

  float_t margin = 0.35f;

  SpatialData spatial{
    .transform = spriteTransform(Vec2f{ pos[0] + margin, pos[1] }, charSize),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(counterId, spatial);

  DynamicTextData render{
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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
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

  float_t value = static_cast<float_t>(m_difficultyLevel) / MAX_DIFFICULTY_LEVEL;
  m_gameDifficultySlider = constructSlider(id, { 0.65f, 0.55f }, value);

  GameOptions options = getOptionsForLevel(0);
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

  auto behaviour = createBGeneric(hashString("menu_behaviour"),
    { g_strMenuItemActivate, g_strSubmenuExit },
    [this, id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

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
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  m_pauseMenuId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuRootSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_pauseMenuId, pauseMenuRootSpatial);

  m_pauseMenuMain.entityId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuMainSpatial{
    .transform = identityMatrix<float_t, 4>(),
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

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
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

MenuSystemPtr createMenuSystem(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<MenuSystemImpl>(ecs, eventSystem, logger);
}

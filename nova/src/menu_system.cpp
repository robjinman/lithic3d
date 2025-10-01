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

namespace
{

enum class ZIndex : uint32_t
{
  Root,
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
    int difficultyLevel() const override;

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

    float m_sfxVolume = 0.5f;
    float m_musicVolume = 0.5f;
    int m_difficultyLevel = 0;
    float m_musicVolumeDelta = 0.f;
    float m_sfxVolumeDelta = 0.f;

    void constructRoot();
    void constructFlare();
    void constructPauseMenu();
    void constructMainMenu();
    Menu constructSettingsSubmenu(EntityId parent, const Menu& prevMenu);
    Menu constructCreditsSubmenu(const Menu& prevMenu);
    Menu constructGameOptionsSubmenu(const Menu& prevMenu);
    void createAnimations();
    EntityId newMenuItemId();
    void setDifficulty(int level);
  
    void constructMenuItemBase(EntityId id, EntityId parentId, const Sprite& sprite);

    void constructMenuItem(EntityId id, EntityId parent, SysUi::GroupId groupId,
      const Sprite& sprite, const ItemSlots& slots);

    void constructSelector(EntityId id, EntityId parent, SysUi::GroupId groupId,
      const Sprite& sprite, const ItemSlots& slots, const SelectorFunctions& functions);

    EntityId constructTextItem(EntityId parent, const Vec2f& pos, const Vec2f& charSize,
      const std::string& text, const Vec4f& colour);
};

MenuSystemImpl::MenuSystemImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_logger(logger)
{
  createAnimations();

  constructRoot();
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

int MenuSystemImpl::difficultyLevel() const
{
  return m_difficultyLevel;
}

void MenuSystemImpl::setDifficulty(int level)
{
  // TODO
}

void MenuSystemImpl::update()
{
  // TODO
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
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  m_root = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  Vec2f size{ GRID_CELL_W * GRID_W, (5.f + GRID_H) * GRID_CELL_H };
  Vec2f pos{ 0.f, 0.f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(m_root, spatial);

  SpriteData render{
    .textureRect = Rectf{
      .x = pxToUvX(704.f),
      .y = pxToUvY(0.f, 256.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(256.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::Root)
  };

  sysRender.addEntity(m_root, render);
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

  Vec2f size{ 1.3f, 1.5f };
  Vec2f pos{ 0.04f, -0.05f };

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
  ui.inputFilter = { MouseButton::Left, KeyboardKey::Enter, KeyboardKey::Space };
  ui.onInputEnd = [this, &sysAnimation, id](const UserInput&) {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strActivate, [this, id]() {
      m_eventSystem.queueEvent(std::make_unique<EMenuItemActivate>(id));
    });
  };
  ui.onGainFocus = [&sysAnimation, id]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strIdleFocused, true);
  };
  ui.onLoseFocus = [&sysAnimation, id]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strIdle, true);
  };
  ui.onInputBegin = [&sysAnimation, id](const UserInput&) {
    sysAnimation.stopAnimation(id);
    sysAnimation.playAnimation(id, strPrime);
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

  auto focusAll = [&sysAnimation, id, leftBtnId, rightBtnId]() {
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
    MouseButton::Left, KeyboardKey::Enter, KeyboardKey::Space, KeyboardKey::Left, KeyboardKey::Right
  };

  UiData ui{};
  ui.group = groupId;
  ui.inputFilter = filter;
  ui.topSlot = slots.top;
  ui.bottomSlot = slots.bottom;
  ui.onGainFocus = focusAll;
  ui.onLoseFocus = unfocusAll;
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
  ui.onInputEnd = [&sysUi, leftBtnId, rightBtnId](const UserInput& input) {
    if (std::holds_alternative<KeyboardKey>(input)) {
      if (std::get<KeyboardKey>(input) == KeyboardKey::Left) {
        sysUi.sendInputEnd(leftBtnId, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Right) {
        sysUi.sendInputEnd(rightBtnId, input);
      }
    }
  };

  sysUi.addEntity(id, ui);

  auto makeBtnUiComp =
    [this, &sysAnimation, &sysUi, &filter, groupId, focusAll, unfocusAll]
    (EntityId id, const std::function<void()>& onBtnDown, const std::function<void()>& onBtnUp) {

    UiData btnUi{};
    btnUi.group = groupId;
    btnUi.inputFilter = filter;
    btnUi.onGainFocus = focusAll;
    btnUi.onLoseFocus = unfocusAll;
    btnUi.onInputBegin = [&sysAnimation, id, onBtnDown](const UserInput&) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strPrime);
      onBtnDown();
    };
    btnUi.onInputEnd = [&sysAnimation, id, onBtnUp](const UserInput&) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strActivate, onBtnUp);
    };

    sysUi.addEntity(id, btnUi);
  };

  makeBtnUiComp(leftBtnId, functions.leftBtnDown, functions.leftBtnUp);
  makeBtnUiComp(rightBtnId, functions.rightBtnDown, functions.rightBtnUp);
}

Menu MenuSystemImpl::constructSettingsSubmenu(EntityId parent, const Menu& prevMenu)
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
    .parent = parent,
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
    .leftBtnDown = [this]() { m_musicVolumeDelta = -0.1f; },
    .leftBtnUp = [this]() { m_musicVolumeDelta = 0.f; },
    .rightBtnDown = [this]() { m_musicVolumeDelta = 0.1f; },
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
    .leftBtnDown = [this]() { m_sfxVolumeDelta = -0.1f; },
    .leftBtnUp = [this]() { m_sfxVolumeDelta = 0.f; },
    .rightBtnDown = [this]() { m_sfxVolumeDelta = 0.1f; },
    .rightBtnUp = [this]() { m_sfxVolumeDelta = 0.f; }
  };

  constructSelector(sfxVolumeId, id, groupId, sfxVolumeSprite, { musicVolumeId, returnId },
    sfxVolumeFunctions);

  auto musicIcon = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  SpatialData musicIconSpatial{
    .transform = spriteTransform({ 0.75f, 0.55f }, { 0.05f, 0.1f }),
    .parent = id,
    .enabled = true
  };

  sysSpatial.addEntity(musicIcon, musicIconSpatial);

  SpriteData musicIconRender{
    .textureRect = Rectf{
      .x = pxToUvX(96.f),
      .y = pxToUvY(448.f, 64.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(64.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem)
  };

  sysRender.addEntity(musicIcon, musicIconRender);

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

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnId) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
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

  auto mainMenuMain = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData mainMenuMainSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_mainMenu.entityId,
    .enabled = true
  };

  sysSpatial.addEntity(mainMenuMain, mainMenuMainSpatial);

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

  constructMenuItem(m_startGameBtnId, mainMenuMain, m_mainMenu.itemGroupId, startSprite,
    { m_quitGameBtnId, optionsId });

  Sprite optionsSprite{
    .pos{ 0.02f, 0.31f },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(320.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(optionsId, mainMenuMain, m_mainMenu.itemGroupId, optionsSprite,
    { m_startGameBtnId, settingsId });

  Sprite settingsSprite{
    .pos{ 0.02f, 0.21f },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(32.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(settingsId, mainMenuMain, m_mainMenu.itemGroupId, settingsSprite,
    { optionsId, creditsId });

  Sprite creditsSprite{
    .pos{ 0.02f, 0.11f },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(64.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(creditsId, mainMenuMain, m_mainMenu.itemGroupId, creditsSprite,
    { settingsId, m_quitGameBtnId });

  Sprite quitSprite{
    .pos{ 0.02f, 0.01f },
    .size{ 0.3f, 0.05625f },
    .texRect{
      .x = pxToUvX(0.f),
      .y = pxToUvY(192.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }
  };

  constructMenuItem(m_quitGameBtnId, mainMenuMain, m_mainMenu.itemGroupId, quitSprite,
    { creditsId, m_startGameBtnId });

  auto mainMenuSettings = constructSettingsSubmenu(m_mainMenu.entityId,
    { mainMenuMain, m_mainMenu.itemGroupId });
  auto mainMenuOptions = constructGameOptionsSubmenu({ mainMenuMain, m_mainMenu.itemGroupId });
  auto mainMenuCredits = constructCreditsSubmenu({ mainMenuMain, m_mainMenu.itemGroupId });

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [=, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == settingsId) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuSettings.entityId, true);
        sysUi.setActiveGroup(mainMenuSettings.itemGroupId, mainMenuSettings.firstItemId);
      }
      else if (event.entityId == optionsId) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuOptions.entityId, true);
        sysUi.setActiveGroup(mainMenuOptions.itemGroupId, mainMenuOptions.firstItemId);
      }
      else if (event.entityId == creditsId) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuCredits.entityId, true);
        sysUi.setActiveGroup(mainMenuCredits.itemGroupId, mainMenuCredits.firstItemId);
      }
    }
  });

  sysBehaviour.addBehaviour(m_mainMenu.entityId, std::move(behaviour));
}

EntityId MenuSystemImpl::constructTextItem(EntityId parent, const Vec2f& pos, const Vec2f& charSize,
  const std::string& text, const Vec4f& colour)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CSprite
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, charSize),
    .parent = parent,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  TextData render{
    .spriteData{
      .textureRect = {
        .x = pxToUvX(256.f),
        .y = pxToUvY(64.f, 192.f),
        .w = pxToUvW(192.f),
        .h = pxToUvH(192.f)
      },
      .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
      .colour = colour
    },
    .text = text
  };

  sysRender.addEntity(id, render);

  return id;
}

Menu MenuSystemImpl::constructCreditsSubmenu(const Menu& prevMenu)
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

  Vec4f colour{ 0.f, 0.f, 0.f, 1.f };
  Vec2f charSize{ 0.018f, 0.04f };
  constructTextItem(id, { 0.35, 0.7 }, charSize, "Design & Programming: Rob Jinman", colour);
  constructTextItem(id, { 0.35, 0.65 }, charSize, "Music:                Jack Normal", colour);

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

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnId) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId, returnId };
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
  difficultyFunctions.leftBtnUp = [this]() { setDifficulty(m_difficultyLevel - 1); };
  difficultyFunctions.rightBtnUp = [this]() { setDifficulty(m_difficultyLevel + 1); };

  constructSelector(difficultyId, id, groupId, difficultySprite, { returnId, returnId },
    difficultyFunctions);

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

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnId, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnId) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
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

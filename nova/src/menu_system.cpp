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
  EntityId entityId;
  SysUi::GroupId itemGroupId;
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

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    Logger& m_logger;
    EntityId m_root;
    EntityId m_pauseMenu;
    Menu m_pauseMenuMain;
    EntityId m_pauseMenuSettings;
    Menu m_mainMenu;
    EntityId m_startGameBtn;
    EntityId m_resumeBtn;
    EntityId m_quitToMainBtn;
    EntityId m_quitGameBtn;
    AnimationId m_animIdle;
    AnimationId m_animIdleFocused;
    AnimationId m_animPrime;
    AnimationId m_animActivate;
    float m_sfxVolume = 0.5f;
    float m_musicVolume = 0.5f;
    int m_difficultyLevel = 0;

    void constructRoot();
    void constructFlare();
    void constructPauseMenu();
    void constructMainMenu();
    Menu constructSettingsSubmenu(EntityId parent, const Menu& prevMenu);
    Menu constructCreditsSubmenu(const Menu& prevMenu);
    Menu constructGameOptionsSubmenu(const Menu& prevMenu);
    void createAnimations();
    EntityId newMenuItemId();
    void constructMenuItemBase(EntityId id, EntityId parentId, const Vec2f& pos,
      const Vec2f& size, const Rectf& texRect);
    void constructMenuItem(EntityId id, EntityId parent, SysUi::GroupId groupId, const Vec2f& pos,
      const Vec2f& size, const Rectf& texRect, EntityId top, EntityId bottom);
    void constructSelector(EntityId id, EntityId parent, SysUi::GroupId groupId, const Vec2f& pos,
      const Vec2f& size, const Rectf& texRect, EntityId top, EntityId bottom);
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
  return m_startGameBtn;
}

EntityId MenuSystemImpl::resumeBtn() const
{
  return m_resumeBtn;
}

EntityId MenuSystemImpl::quitToMainMenuBtn() const
{
  return m_quitToMainBtn;
}

EntityId MenuSystemImpl::quitGameBtn() const
{
  return m_quitGameBtn;
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

void MenuSystemImpl::createAnimations()
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto animIdle = std::unique_ptr<Animation>(new Animation{
    .name = strIdle,
    .duration = 20,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.8f, 0.8f, 0.8f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.6f, 0.6f, 0.8f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.8f, 0.8f, 0.8f, 1.f }
      }
    }
  });

  m_animIdle = sysAnimation.addAnimation(std::move(animIdle));

  auto animIdleFocused = std::unique_ptr<Animation>(new Animation{
    .name = strIdleFocused,
    .duration = 20,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.9f, 0.45f, 0.45f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.8f, 0.35f, 0.35f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.6f, 0.25f, 0.25f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.8f, 0.35f, 0.35f, 1.f }
      }
    }
  });

  m_animIdleFocused = sysAnimation.addAnimation(std::move(animIdleFocused));

  auto animPrime = std::unique_ptr<Animation>(new Animation{
    .name = strPrime,
    .duration = 1,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.4f, 0.f, 0.f, 1.f }
      }
    }
  });

  m_animPrime = sysAnimation.addAnimation(std::move(animPrime));

  auto animActivate = std::unique_ptr<Animation>(new Animation{
    .name = strActivate,
    .duration = 20,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 0.4f, 0.f, 0.f, 1.f }
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 0.6f, 0.6f, 1.f }
      }
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
  sysSpatial.setEnabled(m_pauseMenu, true);
  sysSpatial.setEnabled(m_pauseMenuMain.entityId, true);
  sysSpatial.setEnabled(m_pauseMenuSettings, false);

  sysUi.setActiveGroup(m_pauseMenuMain.itemGroupId);
}

void MenuSystemImpl::showMainMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  sysSpatial.setEnabled(m_pauseMenu, false);
  sysSpatial.setEnabled(m_mainMenu.entityId, true);

  sysUi.setActiveGroup(m_mainMenu.itemGroupId);
}

void MenuSystemImpl::constructRoot()
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  m_root = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
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
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
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
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite, CUi
  >();
}

void MenuSystemImpl::constructMenuItemBase(EntityId id, EntityId parentId, const Vec2f& pos,
  const Vec2f& size, const Rectf& texRect)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = texRect,
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
  const Vec2f& pos, const Vec2f& size, const Rectf& texRect, EntityId top, EntityId bottom)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  constructMenuItemBase(id, parentId, pos, size, texRect);

  UiData ui{};
  ui.group = groupId;
  ui.topSlot = top;
  ui.bottomSlot = bottom;
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
  const Vec2f& pos, const Vec2f& size, const Rectf& texRect, EntityId top, EntityId bottom)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  constructMenuItemBase(id, parentId, pos, size, texRect);

  auto leftBtn = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite, CUi
  >();

  constructMenuItemBase(leftBtn, parentId, { 0.4f, pos[1] }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(192.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto rightBtn = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite, CUi
  >();

  constructMenuItemBase(rightBtn, parentId, { 0.5f, pos[1] }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(224.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto focusAll = [&sysAnimation, id, leftBtn, rightBtn]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtn);
    sysAnimation.stopAnimation(rightBtn);
    sysAnimation.playAnimation(id, strIdleFocused, true);
    sysAnimation.playAnimation(leftBtn, strIdleFocused, true);
    sysAnimation.playAnimation(rightBtn, strIdleFocused, true);
  };

  auto unfocusAll = [&sysAnimation, id, leftBtn, rightBtn]() {
    sysAnimation.stopAnimation(id);
    sysAnimation.stopAnimation(leftBtn);
    sysAnimation.stopAnimation(rightBtn);
    sysAnimation.playAnimation(id, strIdle, true);
    sysAnimation.playAnimation(leftBtn, strIdle, true);
    sysAnimation.playAnimation(rightBtn, strIdle, true);
  };

  std::set<UserInput> filter{
    MouseButton::Left, KeyboardKey::Enter, KeyboardKey::Space, KeyboardKey::Left, KeyboardKey::Right
  };

  UiData ui{};
  ui.group = groupId;
  ui.inputFilter = filter;
  ui.topSlot = top;
  ui.bottomSlot = bottom;
  ui.onGainFocus = focusAll;
  ui.onLoseFocus = unfocusAll;
  ui.onInputBegin = [&sysUi, leftBtn, rightBtn](const UserInput& input) {
    if (std::holds_alternative<KeyboardKey>(input)) {
      if (std::get<KeyboardKey>(input) == KeyboardKey::Left) {
        sysUi.sendInputBegin(leftBtn, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Right) {
        sysUi.sendInputBegin(rightBtn, input);
      }
    }
  };
  ui.onInputEnd = [&sysUi, leftBtn, rightBtn](const UserInput& input) {
    if (std::holds_alternative<KeyboardKey>(input)) {
      if (std::get<KeyboardKey>(input) == KeyboardKey::Left) {
        sysUi.sendInputEnd(leftBtn, input);
      }
      else if (std::get<KeyboardKey>(input) == KeyboardKey::Right) {
        sysUi.sendInputEnd(rightBtn, input);
      }
    }
  };

  sysUi.addEntity(id, ui);

  auto makeBtnUiComp =
    [this, &sysAnimation, &sysUi, &filter, groupId, focusAll, unfocusAll](EntityId id) {

    UiData btnUi{};
    btnUi.group = groupId;
    btnUi.inputFilter = filter;
    btnUi.onGainFocus = focusAll;
    btnUi.onLoseFocus = unfocusAll;
    btnUi.onInputBegin = [&sysAnimation, id](const UserInput&) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strPrime);
    };
    btnUi.onInputEnd = [this, &sysAnimation, id](const UserInput&) {
      sysAnimation.stopAnimation(id);
      sysAnimation.playAnimation(id, strActivate, [this, id]() {
        m_eventSystem.queueEvent(std::make_unique<EMenuItemActivate>(id));
      });
    };

    sysUi.addEntity(id, btnUi);
  };

  makeBtnUiComp(leftBtn);
  makeBtnUiComp(rightBtn);
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

  auto musicVolume = newMenuItemId();
  auto sfxVolume = newMenuItemId();
  auto returnBtn = newMenuItemId();

  auto groupId = SysUi::nextGroupId();

  constructSelector(musicVolume, id, groupId, { 0.02f, 0.21f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(96.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  }, NULL_ENTITY, sfxVolume);

  constructSelector(sfxVolume, id, groupId, { 0.02f, 0.11f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(128.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  }, musicVolume, returnBtn);

  auto musicIcon = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
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

  constructMenuItem(returnBtn, id, groupId, { 0.02f, 0.01f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(160.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  }, sfxVolume, NULL_ENTITY);

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnBtn, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnBtn) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId };
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

  m_startGameBtn = newMenuItemId();
  auto options = newMenuItemId();
  auto settings = newMenuItemId();
  auto credits = newMenuItemId();
  m_quitGameBtn = newMenuItemId();

  constructMenuItem(m_startGameBtn, mainMenuMain, m_mainMenu.itemGroupId, { 0.02f, 0.41f },
    { 0.3f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(0.f, 32.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(32.f)
    }, NULL_ENTITY, options);

  constructMenuItem(options, mainMenuMain, m_mainMenu.itemGroupId, { 0.02f, 0.31f },
    { 0.3f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(320.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, m_startGameBtn, settings);

  constructMenuItem(settings, mainMenuMain, m_mainMenu.itemGroupId, { 0.02f, 0.21f },
    { 0.3f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(32.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, options, credits);

  constructMenuItem(credits, mainMenuMain, m_mainMenu.itemGroupId, { 0.02f, 0.11f },
    { 0.3f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(64.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, settings, m_quitGameBtn);

  constructMenuItem(m_quitGameBtn, mainMenuMain, m_mainMenu.itemGroupId, { 0.02f, 0.01f },
    { 0.3f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(192.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, credits, NULL_ENTITY);

  auto mainMenuSettings = constructSettingsSubmenu(m_mainMenu.entityId,
    { mainMenuMain, m_mainMenu.itemGroupId });
  auto mainMenuOptions = constructGameOptionsSubmenu({ mainMenuMain, m_mainMenu.itemGroupId });
  auto mainMenuCredits = constructCreditsSubmenu({ mainMenuMain, m_mainMenu.itemGroupId });

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [=, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == settings) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuSettings.entityId, true);
        sysUi.setActiveGroup(mainMenuSettings.itemGroupId);
      }
      else if (event.entityId == options) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuOptions.entityId, true);
        sysUi.setActiveGroup(mainMenuOptions.itemGroupId);
      }
      else if (event.entityId == credits) {
        sysSpatial.setEnabled(mainMenuMain, false);
        sysSpatial.setEnabled(mainMenuCredits.entityId, true);
        sysUi.setActiveGroup(mainMenuCredits.itemGroupId);
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
    CLocalTransform, CGlobalTransform, CSpatialFlags, CSprite
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, charSize),
    .parent = parent,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  SpriteData render{
    .textureRect = {
      .x = pxToUvX(256.f),
      .y = pxToUvY(64.f, 192.f),
      .w = pxToUvW(192.f),
      .h = pxToUvH(192.f)
    },
    .zIndex = static_cast<uint32_t>(ZIndex::MenuItem),
    .colour = colour,
    .text = text
  };

  sysRender.addEntity(id, render);

  return id;
}

Menu MenuSystemImpl::constructCreditsSubmenu(const Menu& prevMenu)
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
    .parent = m_mainMenu.entityId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  Vec4f colour{ 0.f, 0.f, 0.f, 1.f };
  Vec2f charSize{ 0.018f, 0.04f };
  constructTextItem(id, { 0.35, 0.7 }, charSize, "Design & Programming: Rob Jinman", colour);
  constructTextItem(id, { 0.35, 0.65 }, charSize, "Music:                Jack Normal", colour);

  auto groupId = SysUi::nextGroupId();

  auto returnBtn = newMenuItemId();

  constructMenuItem(returnBtn, id, groupId, { 0.02f, 0.01f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(160.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  }, NULL_ENTITY, NULL_ENTITY);

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnBtn, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnBtn) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId };
}

Menu MenuSystemImpl::constructGameOptionsSubmenu(const Menu& prevMenu)
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
    .parent = m_mainMenu.entityId,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  auto groupId = SysUi::nextGroupId();

  auto difficultyUp = newMenuItemId();
  auto difficultyDown = newMenuItemId();
  auto returnBtn = newMenuItemId();

  constructMenuItem(difficultyUp, id, groupId, { 0.5f, 0.11f }, { 0.075, 0.05625f }, {
    .x = pxToUvX(224.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  }, NULL_ENTITY, NULL_ENTITY);

  constructMenuItem(difficultyDown, id, groupId, { 0.4f, 0.11f }, { 0.075, 0.05625f }, {
    .x = pxToUvX(192.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  }, NULL_ENTITY, NULL_ENTITY);

  constructMenuItem(returnBtn, id, groupId, { 0.02f, 0.01f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(160.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  }, NULL_ENTITY, NULL_ENTITY);

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [this, id, prevMenu, returnBtn, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == returnBtn) {
        sysSpatial.setEnabled(id, false);
        sysSpatial.setEnabled(prevMenu.entityId, true);
        sysUi.setActiveGroup(prevMenu.itemGroupId);
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return { id, groupId };
}

void MenuSystemImpl::constructPauseMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  m_pauseMenu = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuRootSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_pauseMenu, pauseMenuRootSpatial);

  m_pauseMenuMain.entityId = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuMainSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_pauseMenu,
    .enabled = true
  };

  sysSpatial.addEntity(m_pauseMenuMain.entityId, pauseMenuMainSpatial);

  m_pauseMenuMain.itemGroupId = SysUi::nextGroupId();

  m_resumeBtn = newMenuItemId();
  auto settings = newMenuItemId();
  m_quitToMainBtn = newMenuItemId();
  
  constructMenuItem(m_resumeBtn, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    { 0.02f, 0.21f }, { 0.4f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(224.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, NULL_ENTITY, settings);

  constructMenuItem(settings, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    { 0.02f, 0.11f }, { 0.4f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(32.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, m_resumeBtn, m_quitToMainBtn);

  constructMenuItem(m_quitToMainBtn, m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId,
    { 0.02f, 0.01f }, { 0.4f, 0.05625f }, {
      .x = pxToUvX(0.f),
      .y = pxToUvY(256.f, 32.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(32.f)
    }, settings, NULL_ENTITY);

  auto pauseMenuSettings = constructSettingsSubmenu(m_pauseMenu,
    { m_pauseMenuMain.entityId, m_pauseMenuMain.itemGroupId });
  m_pauseMenuSettings = pauseMenuSettings.entityId;

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strMenuItemActivate },
    [=, this, &sysSpatial, &sysUi](const Event& e) {

    if (e.name == g_strMenuItemActivate) {
      auto& event = dynamic_cast<const EMenuItemActivate&>(e);
      if (event.entityId == settings) {
        sysSpatial.setEnabled(m_pauseMenuMain.entityId, false);
        sysSpatial.setEnabled(m_pauseMenuSettings, true);
        sysUi.setActiveGroup(pauseMenuSettings.itemGroupId);
      }
    }
  });

  sysBehaviour.addBehaviour(m_pauseMenu, std::move(behaviour));
}

}

MenuSystemPtr createMenuSystem(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<MenuSystemImpl>(ecs, eventSystem, logger);
}

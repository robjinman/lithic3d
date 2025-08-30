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
#include "input_state.hpp"
#include "b_generic.hpp"

namespace
{

static const HashedString strIdle = hashString("idle");
static const HashedString strIdleActive = hashString("idle_active");

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size)
{
  return translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

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
    EntityId m_pauseMenuMain;
    EntityId m_pauseMenuSettings;
    EntityId m_mainMenu;
    EntityId m_startGameBtn;
    EntityId m_resumeBtn;
    EntityId m_quitToMainBtn;
    EntityId m_quitGameBtn;
    AnimationId m_animIdle;
    AnimationId m_animIdleActive;
    float m_sfxVolume = 0.5f;
    float m_musicVolume = 0.5f;
    int m_difficultyLevel = 0;

    void constructRoot();
    void constructPauseMenu();
    void constructMainMenu();
    EntityId constructSettingsSubmenu();
    EntityId constructCreditsSubmenu();
    EntityId constructGameOptionsSubmenu();
    void createAnimations();
    EntityId constructMenuItem(EntityId parent, const Vec2f& pos, const Vec2f& size,
      const Rectf& texRect);
};

MenuSystemImpl::MenuSystemImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_logger(logger)
{
  createAnimations();

  constructRoot();
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
    .duration = 60,
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
        .colour = Vec4f{ 1.f, 0.f, 1.f, 1.f }
      }
    }
  });

  m_animIdle = sysAnimation.addAnimation(std::move(animIdle));

  auto animIdleActive = std::unique_ptr<Animation>(new Animation{
    .name = strIdleActive,
    .duration = 60,
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
        .colour = Vec4f{ 1.f, 0.f, 0.f, 1.f }
      }
    }
  });

  m_animIdleActive = sysAnimation.addAnimation(std::move(animIdleActive));
}

EntityId MenuSystemImpl::root() const
{
  return m_root;
}

void MenuSystemImpl::showPauseMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  sysSpatial.setEnabled(m_mainMenu, false);
  sysSpatial.setEnabled(m_pauseMenu, true);
  sysSpatial.setEnabled(m_pauseMenuMain, true);
  sysSpatial.setEnabled(m_pauseMenuSettings, false);
}

void MenuSystemImpl::showMainMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  sysSpatial.setEnabled(m_pauseMenu, false);
  sysSpatial.setEnabled(m_mainMenu, true);
}

void MenuSystemImpl::constructRoot()
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  m_root = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender
  >();

  Vec2f size{ GRID_CELL_W * GRID_W, (5.f + GRID_H) * GRID_CELL_H };
  Vec2f pos{ 0.f, 0.f };

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = sysSpatial.root(),
    .enabled = false
  };

  sysSpatial.addEntity(m_root, spatial);

  RenderData render{
    .textureRect = Rectf{
      .x = pxToUvX(704.f),
      .y = pxToUvY(0.f, 256.f),
      .w = pxToUvW(256.f),
      .h = pxToUvH(256.f)
    },
    .zIndex = 100
  };

  sysRender.addEntity(m_root, render);
}

EntityId MenuSystemImpl::constructMenuItem(EntityId parentId, const Vec2f& pos, const Vec2f& size,
  const Rectf& texRect)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));
  auto& sysUi = dynamic_cast<SysUi&>(m_ecs.system(UI_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender, CUi
  >();

  SpatialData spatial{
    .transform = spriteTransform(pos, size),
    .parent = parentId,
    .enabled = true
  };

  sysSpatial.addEntity(id, spatial);

  RenderData render{
    .textureRect = texRect,
    .zIndex = 102
  };

  sysRender.addEntity(id, render);

  sysAnimation.addEntity(id, AnimationData{
    .animations = { m_animIdle }
  });
  sysAnimation.playAnimation(id, strIdle, true);

  UiData ui{};

  sysUi.addEntity(id, ui);

  return id;
}

void MenuSystemImpl::constructMainMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));

  m_mainMenu = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRender
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_root,
    .enabled = true
  };

  sysSpatial.addEntity(m_mainMenu, spatial);

  m_startGameBtn = constructMenuItem(m_mainMenu, { 0.02f, 0.41f }, { 0.3f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(192.f),
    .h = pxToUvH(32.f)
  });

  auto options = constructMenuItem(m_mainMenu, { 0.02f, 0.31f }, { 0.3f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(320.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  m_quitGameBtn = constructMenuItem(m_mainMenu, { 0.02f, 0.01f }, { 0.3f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(192.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  auto credits = constructMenuItem(m_mainMenu, { 0.02f, 0.11f }, { 0.3f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(64.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  auto settings = constructMenuItem(m_mainMenu, { 0.02f, 0.21f }, { 0.3f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(32.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });
}

EntityId MenuSystemImpl::constructSettingsSubmenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  auto id = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData spatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_pauseMenu,
    .enabled = false
  };

  sysSpatial.addEntity(id, spatial);

  auto sfxVolume = constructMenuItem(id, { 0.02f, 0.11f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(128.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  auto sfxVolumeUp = constructMenuItem(id, { 0.5f, 0.11f }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(224.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto sfxVolumeDown = constructMenuItem(id, { 0.4f, 0.11f }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(192.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto musicVolume = constructMenuItem(id, { 0.02f, 0.21f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(96.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  auto musicVolumeUp = constructMenuItem(id, { 0.5f, 0.21f }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(224.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto musicVolumeDown = constructMenuItem(id, { 0.4f, 0.21f }, { 0.075f, 0.05625f }, {
    .x = pxToUvX(192.f),
    .y = pxToUvY(0.f, 32.f),
    .w = pxToUvW(32.f),
    .h = pxToUvH(32.f)
  });

  auto returnBtn = constructMenuItem(id, { 0.02f, 0.01f }, { 0.4, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(160.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strUiItemActivate },
    [this, returnBtn, &sysSpatial](const Event& e) {

    if (e.name == g_strUiItemActivate) {
      auto& event = dynamic_cast<const EUiItemActivate&>(e);
      if (event.entityId == returnBtn) {
        sysSpatial.setEnabled(m_pauseMenuSettings, false);
        sysSpatial.setEnabled(m_pauseMenuMain, true);
      }
    }
  });

  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return id;
}

void MenuSystemImpl::constructPauseMenu()
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(m_ecs.system(SPATIAL_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(m_ecs.system(BEHAVIOUR_SYSTEM));

  m_pauseMenu = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuRootSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_root,
    .enabled = false
  };

  sysSpatial.addEntity(m_pauseMenu, pauseMenuRootSpatial);

  m_pauseMenuMain = m_ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags
  >();

  SpatialData pauseMenuMainSpatial{
    .transform = identityMatrix<float_t, 4>(),
    .parent = m_pauseMenu,
    .enabled = true
  };

  sysSpatial.addEntity(m_pauseMenuMain, pauseMenuMainSpatial);

  // Resume
  m_resumeBtn = constructMenuItem(m_pauseMenuMain, { 0.02f, 0.21f }, { 0.4f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(224.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  // Settings
  auto settingsItem = constructMenuItem(m_pauseMenuMain, { 0.02f, 0.11f }, { 0.4f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(32.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  // Quit
  m_quitToMainBtn = constructMenuItem(m_pauseMenuMain, { 0.02f, 0.01f }, { 0.4f, 0.05625f }, {
    .x = pxToUvX(0.f),
    .y = pxToUvY(256.f, 32.f),
    .w = pxToUvW(256.f),
    .h = pxToUvH(32.f)
  });

  m_pauseMenuSettings = constructSettingsSubmenu();

  auto behaviour = createBGeneric(hashString("menu_behaviour"), { g_strUiItemActivate },
    [this, settingsItem, &sysSpatial](const Event& e) {

    if (e.name == g_strUiItemActivate) {
      auto& event = dynamic_cast<const EUiItemActivate&>(e);
      if (event.entityId == settingsItem) {
        sysSpatial.setEnabled(m_pauseMenuMain, false);
        sysSpatial.setEnabled(m_pauseMenuSettings, true);
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

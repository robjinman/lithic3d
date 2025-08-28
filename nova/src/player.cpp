#include "player.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include "sys_animation.hpp"
#include "event_system.hpp"
#include "game_events.hpp"
#include "events.hpp"
#include "systems.hpp"

namespace
{

// TODO: Move this
Mat4x4f spriteTransform(const Vec2f& pos, const Vec2f& size)
{
  return translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

class PlayerBehaviour : public CBehaviour
{
  public:
    PlayerBehaviour(EntityId entityId, SysGrid& grid, EventSystem& eventSystem);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    EntityId m_entityId;
    SysGrid& m_sysGrid;
    EventSystem& m_eventSystem;
};

PlayerBehaviour::PlayerBehaviour(EntityId entityId, SysGrid& sysGrid, EventSystem& eventSystem)
  : m_entityId(entityId)
  , m_sysGrid(sysGrid)
  , m_eventSystem(eventSystem)
{
}

HashedString PlayerBehaviour::name() const
{
  static HashedString strUserControl = hashString("player");
  return strUserControl;
}

const std::set<HashedString>& PlayerBehaviour::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityExplode
  };
  return subs;
}

void PlayerBehaviour::processEvent(const Event& event)
{
  if (event.name == g_strEntityExplode) {
    auto& e = dynamic_cast<const EEntityExplode&>(event);
    if (m_sysGrid.hasEntityAt(m_entityId, e.pos[0], e.pos[1])) {
      m_eventSystem.queueEvent(std::make_unique<EPlayerDeath>());
      m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
    }
  }
}

} // namespace

EntityId constructPlayer(EventSystem& eventSystem, Ecs& ecs, EntityId worldRoot)
{
  auto& sysSpatial = dynamic_cast<SysSpatial&>(ecs.system(SPATIAL_SYSTEM));
  auto& sysRender = dynamic_cast<SysRender&>(ecs.system(RENDER_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(ecs.system(ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(ecs.system(GRID_SYSTEM));
  auto& sysBehaviour = dynamic_cast<SysBehaviour&>(ecs.system(BEHAVIOUR_SYSTEM));

  auto id = ecs.componentStore().allocate<
    CLocalTransform, CGlobalTransform, CSpatialFlags, CRenderView
  >();

  sysGrid.addEntity(id, 0, 0);

  Vec2f size{ 0.0625f, 0.0625f };
  Vec2f pos{ 0.f, 0.f };

  CSpatial spatial{
    .transform = spriteTransform(pos, size),
    .parent = worldRoot,
    .isActive = true
  };

  sysSpatial.addEntity(id, spatial);

  CRender render{
    .textureRect = Rectf{
      .x = pxToUvX(384.f),
      .y = pxToUvY(256.f, 48.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(48.f)
    },
    .zIndex = 2
  };

  sysRender.addEntity(id, render);

  long animationDuration = 16;
  float_t delta = 0.015625f;

  auto animMoveLeft = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_left"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ -delta * 1.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -delta * 2.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -delta * 3.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ -delta * 4.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(480.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      }
    }
  });

  auto animMoveRight = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_right"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ delta * 1.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ delta * 2.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ delta * 3.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ delta * 4.f, 0.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(480.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      }
    }
  });

  auto animMoveUp = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_up"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, delta * 1.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, delta * 2.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, delta * 3.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, delta * 4.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(480.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      }
    }
  });

  auto animMoveDown = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_down"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .pos = Vec2f{ 0.f, -delta * 1.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, -delta * 2.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, -delta * 3.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .pos = Vec2f{ 0.f, -delta * 4.f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = Rectf{
          .x = pxToUvX(480.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      }
    }
  });

  sysAnimation.addEntity(id, CAnimation{
    .animations = {
      sysAnimation.addAnimation(std::move(animMoveLeft)),
      sysAnimation.addAnimation(std::move(animMoveRight)),
      sysAnimation.addAnimation(std::move(animMoveUp)),
      sysAnimation.addAnimation(std::move(animMoveDown))
    }
  });

  auto behaviour = std::make_unique<PlayerBehaviour>(id, sysGrid, eventSystem);
  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return id;
}

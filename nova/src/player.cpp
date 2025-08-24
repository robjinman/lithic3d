#include "player.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "sys_animation.hpp"
#include "event_system.hpp"
#include "game_events.hpp"

namespace
{

class PlayerBehaviour : public CBehaviour
{
  public:
    PlayerBehaviour(EntityId entityId, SysGrid& grid, EventSystem& eventSystem);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;

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

void PlayerBehaviour::processEvent(const GameEvent& event)
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

EntityId constructPlayer(EventSystem& eventSystem, ComponentStore& componentStore, SysGrid& sysGrid,
  SysRender& sysRender, SysBehaviour& sysBehaviour, SysAnimation& sysAnimation)
{
  auto id = componentStore.allocate<CRenderView>();

  sysGrid.addEntity(id, 0, 0);

  CRender render{
    .textureRect = Rectf{
      .x = pxToUvX(384.f),
      .y = pxToUvY(256.f, 48.f),
      .w = pxToUvW(32.f),
      .h = pxToUvH(48.f)
    },
    .size = Vec2f{ 0.0625f, 0.0625f },
    .pos = Vec2f{ 0.f, 0.f },
    .zIndex = 2
  };

  sysRender.addEntity(id, render);

  long animationDuration = 16;

  auto animMoveLeft = std::unique_ptr<Animation>(new Animation{
    .name = hashString("move_left"),
    .duration = animationDuration,
    .frames = {
      AnimationFrame{
        .delta = Vec2f{ -0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ -0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ -0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(352.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ -0.015625f, 0.f },
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
        .delta = Vec2f{ 0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.015625f, 0.f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(304.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.015625f, 0.f },
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
        .delta = Vec2f{ 0.f, 0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(256.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, 0.015625f },
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
        .delta = Vec2f{ 0.f, -0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(384.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, -0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(416.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, -0.015625f },
        .textureRect = Rectf{
          .x = pxToUvX(448.f),
          .y = pxToUvY(400.f, 48.f),
          .w = pxToUvW(32.f),
          .h = pxToUvH(48.f)
        },
        .colour = std::nullopt
      },
      AnimationFrame{
        .delta = Vec2f{ 0.f, -0.015625f },
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

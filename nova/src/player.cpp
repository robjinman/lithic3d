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
    PlayerBehaviour(EntityId entityId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;

  private:
    EntityId m_entityId;
};

PlayerBehaviour::PlayerBehaviour(EntityId entityId)
  : m_entityId(entityId)
{
}

HashedString PlayerBehaviour::name() const
{
  static HashedString strUserControl = hashString("player");
  return strUserControl;
}

const std::set<HashedString>& PlayerBehaviour::subscriptions() const
{
  static std::set<HashedString> subs{};
  return subs;
}

void PlayerBehaviour::processEvent(const GameEvent& event)
{
}

}

EntityId constructPlayer(EventSystem& eventSystem, ComponentStore& componentStore, SysGrid& sysGrid,
  SysRender& sysRender, SysBehaviour& sysBehaviour, SysAnimation& sysAnimation)
{
  auto id = componentStore.allocate<CRenderView, CAnimationView>();

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

  CAnimation animation;
  animation.animations = {
    Animation{
      .name = hashString("move_left"),
      .duration = 10,
      .frames = {
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(384.f),
            .y = pxToUvY(352.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ -0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(416.f),
            .y = pxToUvY(352.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ -0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(448.f),
            .y = pxToUvY(352.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvX(48.f)
          },
          .delta = Vec2f{ -0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(480.f),
            .y = pxToUvY(352.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvX(48.f)
          },
          .delta = Vec2f{ -0.015625f, 0 }
        }
      }
    },
    Animation{
      .name = hashString("move_right"),
      .duration = 10,
      .frames = {
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(384.f),
            .y = pxToUvY(304.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ 0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(416.f),
            .y = pxToUvY(304.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ 0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(448.f),
            .y = pxToUvY(304.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ 0.015625f, 0 }
        },
        AnimationFrame{
          .textureRect = Rectf{
            .x = pxToUvX(480.f),
            .y = pxToUvY(304.f, 48.f),
            .w = pxToUvW(32.f),
            .h = pxToUvH(48.f)
          },
          .delta = Vec2f{ 0.015625f, 0 }
        }
      }
    }
  };

  sysAnimation.addEntity(id, animation);

  auto behaviour = std::make_unique<PlayerBehaviour>(id);
  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return id;
}

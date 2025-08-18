#include "player.hpp"
#include "ecs.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "event_system.hpp"
#include "game_events.hpp"
#include "input_state.hpp"
#include "b_animation.hpp"

namespace
{

class BUserControl : public CBehaviour
{
  public:
    BUserControl(EntityId entityId, EventSystem& eventSystem, SysGrid& sysGrid,
      SysBehaviour& sysBehaviour);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;
    void update(Tick tick, const InputState& inputState) override;

  private:
    EntityId m_entityId;
    EventSystem& m_eventSystem;
    SysGrid& m_sysGrid;
    SysBehaviour& m_sysBehaviour;
};

BUserControl::BUserControl(EntityId entityId, EventSystem& eventSystem, SysGrid& sysGrid,
  SysBehaviour& sysBehaviour)
  : m_entityId(entityId)
  , m_eventSystem(eventSystem)
  , m_sysGrid(sysGrid)
  , m_sysBehaviour(sysBehaviour)
{
}

HashedString BUserControl::name() const
{
  static HashedString strUserControl = hashString("user_control");
  return strUserControl;
}

const std::set<HashedString>& BUserControl::subscriptions() const
{
  static std::set<HashedString> subs{};
  return subs;
}

void BUserControl::processEvent(const GameEvent&)
{
}

void BUserControl::update(Tick, const InputState& inputState)
{
  static auto strAnimation = hashString("animation");
  static auto strMoveLeft = hashString("move_left");
  static auto strMoveRight = hashString("move_right");
  static auto strMoveUp = hashString("move_up");
  static auto strMoveDown = hashString("move_down");

  auto& anim = dynamic_cast<BAnimation&>(m_sysBehaviour.getBehaviour(m_entityId, strAnimation));

  if (anim.hasAnimationPlaying()) {
    return;
  }

  if (inputState.left) {
    if (m_sysGrid.tryMove(m_entityId, -1, 0)) {
      anim.playAnimation(strMoveLeft);
    }
  }
  else if (inputState.right) {
    if (m_sysGrid.tryMove(m_entityId, 1, 0)) {
      anim.playAnimation(strMoveRight);
    }
  }
  else if (inputState.up) {
    if (m_sysGrid.tryMove(m_entityId, 0, 1)) {
      anim.playAnimation(strMoveUp);
    }
  }
  else if (inputState.down) {
    if (m_sysGrid.tryMove(m_entityId, 0, -1)) {
      anim.playAnimation(strMoveDown);
    }
  }
}

}

EntityId constructPlayer(EventSystem& eventSystem, World& world, SysGrid& sysGrid,
  SysRender& sysRender, SysBehaviour& sysBehaviour)
{
  auto id = world.allocate<CRenderView>(); // TODO

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

  auto userCtrlBehaviour = std::make_unique<BUserControl>(id, eventSystem, sysGrid, sysBehaviour);

  Animation moveLeft{
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
  };

  Animation moveRight{
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
  };

  auto animBehaviour = createBAnimation(id, sysRender);
  animBehaviour->addAnimation(std::move(moveLeft));
  animBehaviour->addAnimation(std::move(moveRight));

  sysBehaviour.addBehaviour(id, std::move(userCtrlBehaviour));
  sysBehaviour.addBehaviour(id, std::move(animBehaviour));

  return id;
}

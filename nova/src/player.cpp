#include "player.hpp"
#include "sys_behaviour.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "event_system.hpp"
#include "game_events.hpp"
#include "constants.hpp"
#include "input_state.hpp"

namespace
{

class PlayerBehaviour : public CBehaviour
{
  public:
    PlayerBehaviour(EntityId entityId, EventSystem& eventSystem, SysGrid& sysGrid,
      SysRender& sysRender);

    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;
    void update(const InputState& inputState) override;

  private:
    EntityId m_entityId;
    EventSystem& m_eventSystem;
    SysGrid& m_sysGrid;
    SysRender& m_sysRender;
};

PlayerBehaviour::PlayerBehaviour(EntityId entityId, EventSystem& eventSystem, SysGrid& sysGrid,
  SysRender& sysRender)
  : m_entityId(entityId)
  , m_eventSystem(eventSystem)
  , m_sysGrid(sysGrid)
  , m_sysRender(sysRender)
{
}

const std::set<HashedString>& PlayerBehaviour::subscriptions() const
{
  static std::set<HashedString> subs{};
  return subs;
}

void PlayerBehaviour::processEvent(const GameEvent&)
{
}

void PlayerBehaviour::update(const InputState& inputState)
{
  static auto strRunLeft = hashString("run_left");
  static auto strRunRight = hashString("run_right");
  static auto strRunUp = hashString("run_up");
  static auto strRunDown = hashString("run_down");

  if (m_sysRender.isAnimationPlaying(m_entityId)) {
    return;
  }

  if (inputState.left) {
    if (m_sysGrid.tryMove(m_entityId, -1, 0)) {
      m_sysRender.playAnimation(m_entityId, strRunLeft);
    }
  }
  else if (inputState.right) {
    if (m_sysGrid.tryMove(m_entityId, 1, 0)) {
      m_sysRender.playAnimation(m_entityId, strRunRight);
    }
  }
  else if (inputState.up) {
    if (m_sysGrid.tryMove(m_entityId, 0, 1)) {
      m_sysRender.playAnimation(m_entityId, strRunUp);
    }
  }
  else if (inputState.down) {
    if (m_sysGrid.tryMove(m_entityId, 0, -1)) {
      m_sysRender.playAnimation(m_entityId, strRunDown);
    }
  }
}

}

EntityId constructPlayer(EventSystem& eventSystem, SysGrid& sysGrid, SysRender& sysRender,
  SysBehaviour& sysBehaviour)
{
  auto id = System::nextId();

  sysGrid.addEntity(id, 0, 0);

  float_t offsetXpx = 384.f;
  float_t offsetYpx = 256.f;
  float_t wPx = 32.f;
  float_t hPx = 48.f;
  float_t w = wPx / ATLAS_WIDTH_PX;
  float_t h = hPx / ATLAS_HEIGHT_PX;

  CRender render{
    .textureRect = Rectf{
      .x = offsetXpx / ATLAS_WIDTH_PX,
      .y = (ATLAS_HEIGHT_PX - offsetYpx - hPx) / ATLAS_HEIGHT_PX,
      .w = w,
      .h = h
    },
    .size = Vec2f{ 0.0625f, 0.0625f },
    .pos = Vec2f{ 0.f, 0.f },
    .zIndex = 2
  };

  sysRender.addEntity(id, render);

  auto behaviour = std::make_unique<PlayerBehaviour>(id, eventSystem, sysGrid, sysRender);
  sysBehaviour.addBehaviour(id, std::move(behaviour));

  return id;
}

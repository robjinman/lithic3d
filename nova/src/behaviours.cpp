#include "behaviours.hpp"
#include "event_system.hpp"
#include "sys_grid.hpp"
#include "sys_render.hpp"
#include "game_events.hpp"

namespace
{

class CollectableBehaviour : public CBehaviour
{
  public:
    CollectableBehaviour(const SysGrid& sysGrid, SysRender& sysRender, EventSystem& eventSystem,
      EntityId entityId, EntityId playerId, int value)
      : m_sysGrid(sysGrid)
      , m_sysRender(sysRender)
      , m_eventSystem(eventSystem)
      , m_entityId(entityId)
      , m_playerId(playerId)
      , m_value(value)
    {}

    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;
    void update() override {}

  private:
    const SysGrid& m_sysGrid;
    SysRender& m_sysRender;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    uint32_t m_value = 0;
};

const std::set<HashedString>& CollectableBehaviour::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strPlayerMoved,
    g_strAnimationFinished
  };
  return subs;
}

void CollectableBehaviour::processEvent(const GameEvent& event)
{
  static HashedString strCollect = hashString("collect");

  if (event.name == g_strPlayerMoved) {
    auto& e = dynamic_cast<const EPlayerMoved&>(event);

    if (m_sysGrid.hasEntityAt(e.toPos[0], e.toPos[1], m_entityId)) {
      m_sysRender.playAnimation(m_entityId, strCollect);
      m_eventSystem.fireEvent(EItemCollected{m_entityId, m_value});
    }
  }
  else if (event.name == g_strAnimationFinished) {
    auto& e = dynamic_cast<const EAnimationFinished&>(event);

    if (e.entityId == m_entityId && e.name == strCollect) {
      m_eventSystem.fireEvent(ERequestDeletion{m_entityId});
    }
  }
}

}

CBehaviourPtr createCollectableBehaviour(const SysGrid& sysGrid, SysRender& sysRender,
  EventSystem& eventSystem, EntityId entityId, EntityId playerId, uint32_t value)
{
  return std::make_unique<CollectableBehaviour>(sysGrid, sysRender, eventSystem, entityId, playerId,
    value);
}

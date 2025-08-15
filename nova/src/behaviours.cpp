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
    CollectableBehaviour(SysRender& sysRender, EventSystem& eventSystem, EntityId entityId,
      EntityId playerId, int value);

    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;
    void update(const InputState&) override {}

  private:
    SysRender& m_sysRender;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    uint32_t m_value = 0;
};

CollectableBehaviour::CollectableBehaviour(SysRender& sysRender, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value)
  : m_sysRender(sysRender)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_playerId(playerId)
  , m_value(value)
{}

const std::set<HashedString>& CollectableBehaviour::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strPlayerStepOn,
    g_strAnimationFinished
  };
  return subs;
}

void CollectableBehaviour::processEvent(const GameEvent& event)
{
  static HashedString strCollect = hashString("collect");

  if (event.name == g_strPlayerStepOn) {
    auto& e = dynamic_cast<const EEntityStepOn&>(event);

    if (e.entityId == m_playerId) {
      m_sysRender.playAnimation(m_entityId, strCollect);
      m_eventSystem.fireEvent(EItemCollect{m_entityId, m_value});
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

CBehaviourPtr createCollectableBehaviour(SysRender& sysRender, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, uint32_t value)
{
  return std::make_unique<CollectableBehaviour>(sysRender, eventSystem, entityId, playerId, value);
}

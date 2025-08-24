#include "b_collectable.hpp"
#include "event_system.hpp"
#include "sys_animation.hpp"
#include "game_events.hpp"

namespace
{

class BCollectable : public CBehaviour
{
  public:
    BCollectable(SysAnimation& sysAnimation, EventSystem& eventSystem, EntityId entityId,
      EntityId playerId, int value);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;

  private:
    SysAnimation& m_sysAnimation;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    uint32_t m_value = 0;
};

BCollectable::BCollectable(SysAnimation& sysAnimation, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, int value)
  : m_sysAnimation(sysAnimation)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_playerId(playerId)
  , m_value(value)
{}

HashedString BCollectable::name() const
{
  static auto strCollectable = hashString("collectable");
  return strCollectable;
}

const std::set<HashedString>& BCollectable::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityStepOn,
    g_strAnimationFinish
  };
  return subs;
}

void BCollectable::processEvent(const GameEvent& event)
{
  static HashedString strCollect = hashString("collect");

  if (event.name == g_strEntityStepOn) {
    auto& e = dynamic_cast<const EEntityStepOn&>(event);

    if (e.entityId == m_playerId) {
      m_sysAnimation.playAnimation(m_entityId, strCollect);
      m_eventSystem.queueEvent(std::make_unique<EItemCollect>(m_entityId, m_value));
    }
  }
  else if (event.name == g_strAnimationFinish) {
    auto& e = dynamic_cast<const EAnimationFinish&>(event);

    if (e.animationName == strCollect) {
      m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
    }
  }
}

} // namespace

CBehaviourPtr createBCollectable(SysAnimation& sysAnimation, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value)
{
  return std::make_unique<BCollectable>(sysAnimation, eventSystem, entityId, playerId, value);
}

#include "b_collectable.hpp"
#include "event_system.hpp"
#include "sys_animation.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "events.hpp"

namespace
{

class BCollectable : public BehaviourData
{
  public:
    BCollectable(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId,
      int value);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    uint32_t m_value = 0;
};

BCollectable::BCollectable(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId,
  int value)
  : m_ecs(ecs)
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
    g_strEntityEnter
  };
  return subs;
}

void BCollectable::processEvent(const Event& event)
{
  static HashedString strCollect = hashString("collect");
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  if (event.name == g_strEntityEnter) {
    auto& e = dynamic_cast<const EEntityEnter&>(event);

    if (e.entityId == m_playerId) {
      if (sysAnimation.hasAnimationPlaying(m_entityId)) {
        sysAnimation.finishAnimation(m_entityId);
      }
      sysAnimation.playAnimation(m_entityId, strCollect, [this]() {
        m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
      });
      m_eventSystem.queueEvent(std::make_unique<EItemCollect>(m_entityId, m_value));
    }
  }
}

} // namespace

BehaviourDataPtr createBCollectable(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, int value)
{
  return std::make_unique<BCollectable>(ecs, eventSystem, entityId, playerId, value);
}

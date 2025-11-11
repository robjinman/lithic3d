#include "b_collectable.hpp"
#include "game_events.hpp"
#include <fge/event_system.hpp>
#include <fge/sys_animation_2d.hpp>
#include <fge/systems.hpp>
#include <fge/events.hpp>

using fge::EntityId;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::EventSystem;
using fge::Ecs;
using fge::SysAnimation2d;

namespace
{

class BCollectable : public fge::DBehaviour
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
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  if (event.name == g_strEntityEnter) {
    auto& e = dynamic_cast<const EEntityEnter&>(event);

    if (e.entityId == m_playerId) {
      if (sysAnimation.hasAnimationPlaying(m_entityId)) {
        sysAnimation.finishAnimation(m_entityId);
      }
      sysAnimation.playAnimation(m_entityId, strCollect, [this]() {
        m_eventSystem.raiseEvent(fge::ERequestDeletion{m_entityId});
      });
      m_eventSystem.raiseEvent(EItemCollect{m_entityId, m_value});
    }
  }
}

} // namespace

fge::DBehaviourPtr createBCollectable(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, int value)
{
  return std::make_unique<BCollectable>(ecs, eventSystem, entityId, playerId, value);
}

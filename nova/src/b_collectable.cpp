#include "b_collectable.hpp"
#include "event_system.hpp"
#include "sys_grid.hpp"
#include "game_events.hpp"
#include "b_animation.hpp"

namespace
{

class BCollectable : public CBehaviour
{
  public:
    BCollectable(SysBehaviour& sysBehaviour, EventSystem& eventSystem, EntityId entityId,
      EntityId playerId, int value);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent& event) override;
    void update(Tick tick, const InputState&) override {}

  private:
    SysBehaviour& m_sysBehaviour;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    uint32_t m_value = 0;
};

BCollectable::BCollectable(SysBehaviour& sysBehaviour, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value)
  : m_sysBehaviour(sysBehaviour)
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
    g_strAnimationFinished
  };
  return subs;
}

void BCollectable::processEvent(const GameEvent& event)
{
  static HashedString strAnimation = hashString("animation");
  static HashedString strCollect = hashString("collect");

  if (event.name == g_strEntityStepOn) {
    auto& e = dynamic_cast<const EEntityStepOn&>(event);

    if (e.entityId == m_playerId) {
      auto& anim = dynamic_cast<BAnimation&>(m_sysBehaviour.getBehaviour(m_entityId, strAnimation));
      anim.playAnimation(strCollect);
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

CBehaviourPtr createBCollectable(SysBehaviour& sysBehaviour, EventSystem& eventSystem,
  EntityId entityId, EntityId playerId, int value)
{
  return std::make_unique<BCollectable>(sysBehaviour, eventSystem, entityId, playerId, value);
}

#include "b_exit.hpp"
#include "game_events.hpp"
#include <fge/systems.hpp>
#include <fge/sys_animation.hpp>

using fge::EntityId;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::EventSystem;
using fge::Ecs;
using fge::SysAnimation;

namespace
{

class BExit : public fge::BehaviourData
{
  public:
    BExit(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    bool m_open = false;
};

BExit::BExit(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_playerId(playerId)
{
}

HashedString BExit::name() const
{
  static auto strName = hashString("exit");
  return strName;
}

const std::set<HashedString>& BExit::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strGoldTargetAttained,
    g_strEntityLandOn
  };
  return subs;
}

void BExit::processEvent(const Event& event)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(fge::ANIMATION_SYSTEM));

  if (event.name == g_strGoldTargetAttained) {
    sysAnimation.playAnimation(m_entityId, hashString("idle"), true);
    m_open = true;
  }
  else if (event.name == g_strEntityLandOn && m_open) {
    auto& e = dynamic_cast<const EEntityLandOn&>(event);
    if (e.entityId == m_playerId) {
      m_eventSystem.raiseEvent(EEnterPortal{});

      sysAnimation.queueAnimation(m_playerId, hashString("enter_portal"), [this]() {
        m_eventSystem.raiseEvent(EPlayerVictorious{});
      });
    }
  }
}

} // namespace

fge::BehaviourDataPtr createBExit(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId)
{
  return std::make_unique<BExit>(ecs, eventSystem, entityId, playerId);
}

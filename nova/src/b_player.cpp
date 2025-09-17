#include "b_player.hpp"
#include "game_events.hpp"
#include "events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"

namespace
{

class BPlayer : public BehaviourData
{
  public:
    BPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
};

BPlayer::BPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
{
}

HashedString BPlayer::name() const
{
  static HashedString strUserControl = hashString("player");
  return strUserControl;
}

const std::set<HashedString>& BPlayer::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityExplode
  };
  return subs;
}

void BPlayer::processEvent(const Event& event)
{
  if (event.name == g_strEntityExplode) {
    auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

    auto& e = dynamic_cast<const EEntityExplode&>(event);
    if (sysGrid.hasEntityAt(m_entityId, e.pos[0], e.pos[1])) {
      m_eventSystem.queueEvent(std::make_unique<EPlayerDeath>());
      m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
    }
  }
}

} // namespace

BehaviourDataPtr createBPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId)
{
  return std::make_unique<BPlayer>(ecs, eventSystem, entityId);
}

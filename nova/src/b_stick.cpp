#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"

namespace
{

class BStick : public BehaviourData
{
  public:
    BStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EntityId m_entityId;
    EntityId m_playerId;
};

BStick::BStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId)
  : m_ecs(ecs)
  , m_entityId(entityId)
  , m_playerId(playerId)
{
}

HashedString BStick::name() const
{
  static auto strStick = hashString("stick");
  return strStick;
}

const std::set<HashedString>& BStick::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityStepOn
  };
  return subs;
}

void BStick::processEvent(const Event& event)
{

}

} // namespace

BehaviourDataPtr createBStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId)
{
  return std::make_unique<BStick>(ecs, eventSystem, entityId, playerId);
}

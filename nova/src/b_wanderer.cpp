#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"

namespace
{

class BWanderer : public BehaviourData
{
  public:
    BWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EntityId m_entityId;
    EntityId m_playerId;
    bool m_active = false;
};

BWanderer::BWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId)
  : m_ecs(ecs)
  , m_entityId(entityId)
  , m_playerId(playerId)
{
}

HashedString BWanderer::name() const
{
  static auto strWanderer = hashString("wanderer");
  return strWanderer;
}

const std::set<HashedString>& BWanderer::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strPlayerMove
  };
  return subs;
}

void BWanderer::processEvent(const Event& event)
{
  const static HashedString strMoveLeft = hashString("move_left");
  const static HashedString strMoveRight = hashString("move_right");
  const static HashedString strMoveUp = hashString("move_up");
  const static HashedString strMoveDown = hashString("move_down");
  const static HashedString strFadeIn = hashString("fade_in");

  constexpr int sqActivationDist = 36;
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

  if (m_active) {
    if (event.name == g_strAnimationFinish) {
      auto& e = dynamic_cast<const EAnimationFinish&>(event);

      if (e.animationName == strMoveLeft || e.animationName == strMoveRight ||
        e.animationName == strMoveUp || e.animationName == strMoveDown ||
        e.animationName == strFadeIn) {

        // TODO: Move toward player
      }
    }
  }
  else {
    if (event.name == g_strPlayerMove) {
      auto& e = dynamic_cast<const EPlayerMove&>(event);
      auto& pos = sysGrid.entityPos(m_entityId);
      
      auto diff = pos - e.pos;
      int sqDist = diff[0] * diff[0] + diff[1] * diff[1];

      if (sqDist <= sqActivationDist) {
        m_active = true;

        // TODO: Fade in
      }
    }
  }
}

} // namespace

BehaviourDataPtr createBWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId)
{
  return std::make_unique<BWanderer>(ecs, eventSystem, entityId, playerId);
}

#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"
#include "sys_animation.hpp"
#include "sys_render.hpp"

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
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  if (m_active) {
    if (!sysGrid.hasEntity(m_playerId)) {
      m_active = false;
    }
    else if (event.name == g_strAnimationFinish) {
      auto& e = dynamic_cast<const EAnimationFinish&>(event);

      if (e.animationName == strMoveLeft || e.animationName == strMoveRight ||
        e.animationName == strMoveUp || e.animationName == strMoveDown ||
        e.animationName == strFadeIn) {

        auto playerPos = sysGrid.entityPos(m_playerId);
        auto pos = sysGrid.entityPos(m_entityId);

        auto diff = pos - playerPos;
        if (abs(diff[0]) > abs(diff[1])) {
          if (diff[0] > 0) {
            if (sysGrid.tryMove(m_entityId, -1, 0)) {
              sysAnimation.playAnimation(m_entityId, strMoveLeft);
            }
          }
          else if (diff[0] < 0) {
            if (sysGrid.tryMove(m_entityId, 1, 0)) {
              sysAnimation.playAnimation(m_entityId, strMoveRight);
            }
          }
        }
        else {
          if (diff[1] > 0) {
            if (sysGrid.tryMove(m_entityId, 0, -1)) {
              sysAnimation.playAnimation(m_entityId, strMoveDown);
            }
          }
          else if (diff[1] < 0) {
            if (sysGrid.tryMove(m_entityId, 0, 1)) {
              sysAnimation.playAnimation(m_entityId, strMoveUp);
            }
          }
        }
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
        m_ecs.componentStore().component<CSprite>(m_entityId).visible = true;
        sysAnimation.playAnimation(m_entityId, strFadeIn);
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

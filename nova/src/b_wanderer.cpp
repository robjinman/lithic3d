#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"
#include "sys_animation.hpp"
#include "sys_render.hpp"
#include "events.hpp"

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
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    bool m_active = false;

    void makeMove();
};

BWanderer::BWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
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
    g_strPlayerMove,
    g_strEntityExplode,
    g_strEntityLandOn
  };
  return subs;
}

void BWanderer::makeMove()
{
  const static HashedString strMoveLeft = hashString("move_left");
  const static HashedString strMoveRight = hashString("move_right");
  const static HashedString strMoveUp = hashString("move_up");
  const static HashedString strMoveDown = hashString("move_down");

  if (!m_active) {
    return;
  }

  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto pos = sysGrid.entityPos(m_entityId);
  auto entities = sysGrid.getEntities(pos[0], pos[1]);
  entities.erase(m_entityId);
  m_eventSystem.raiseEvent(EEntityLandOn{m_entityId, pos, entities});

  if (!sysGrid.hasEntity(m_playerId)) {
    m_active = false;
    return;
  }

  auto playerPos = sysGrid.entityPos(m_playerId);

  bool moved = false;
  auto diff = pos - playerPos;
  if (abs(diff[0]) > abs(diff[1])) {
    if (diff[0] > 0) {
      if (sysGrid.tryMove(m_entityId, -1, 0)) {
        sysAnimation.playAnimation(m_entityId, strMoveLeft, [this]() { makeMove(); });
        moved = true;
      }
    }
    else if (diff[0] < 0) {
      if (sysGrid.tryMove(m_entityId, 1, 0)) {
        sysAnimation.playAnimation(m_entityId, strMoveRight, [this]() { makeMove(); });
        moved = true;
      }
    }
  }
  else {
    if (diff[1] > 0) {
      if (sysGrid.tryMove(m_entityId, 0, -1)) {
        sysAnimation.playAnimation(m_entityId, strMoveDown, [this]() { makeMove(); });
        moved = true;
      }
    }
    else if (diff[1] < 0) {
      if (sysGrid.tryMove(m_entityId, 0, 1)) {
        sysAnimation.playAnimation(m_entityId, strMoveUp, [this]() { makeMove(); });
        moved = true;
      }
    }
  }

  // tryMove will have triggered an event, which could wind up changing m_active
  if (!m_active) {
    return;
  }

  if (moved) {
    auto newPos = sysGrid.entityPos(m_entityId);
    m_eventSystem.raiseEvent(EAttack{m_entityId, sysGrid.getEntities(newPos[0], newPos[1])});
  }
}

void BWanderer::processEvent(const Event& event)
{
  const static HashedString strFadeIn = hashString("fade_in");

  constexpr int sqActivationDist = 36;
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  if (event.name == g_strEntityExplode) {
    auto& e = dynamic_cast<const EEntityExplode&>(event);

    if (e.pos == sysGrid.entityPos(m_entityId)) {
      m_eventSystem.raiseEvent(ERequestDeletion{m_entityId});
      m_active = false;
      return;
    }
  }

  if (m_active) {
    if (event.name == g_strPlayerMove) {
      if (sysGrid.hasEntity(m_playerId)) {
        auto& playerPos = sysGrid.entityPos(m_playerId);
        auto& pos = sysGrid.entityPos(m_entityId);
        if (playerPos == pos) {
          m_eventSystem.raiseEvent(EAttack{m_entityId, EntityIdSet{ m_playerId }});
        }
      }
    }
  }
  else {
    if (event.name == g_strPlayerMove) {
      if (sysGrid.hasEntity(m_playerId)) {
        auto& e = dynamic_cast<const EPlayerMove&>(event);
        auto& pos = sysGrid.entityPos(m_entityId);

        auto diff = pos - e.pos;
        int sqDist = diff[0] * diff[0] + diff[1] * diff[1];

        if (sqDist <= sqActivationDist) {
          m_active = true;
          m_ecs.componentStore().component<CRender>(m_entityId).visible = true;
          sysAnimation.playAnimation(m_entityId, strFadeIn, [this]() { makeMove(); });
        }
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

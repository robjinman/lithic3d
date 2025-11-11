#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "sys_grid.hpp"
#include <fge/sys_animation_2d.hpp>
#include <fge/sys_render_2d.hpp>
#include <fge/events.hpp>
#include <fge/systems.hpp>

using fge::EntityId;
using fge::HashedString;
using fge::hashString;
using fge::Event;
using fge::EventSystem;
using fge::Ecs;
using fge::SysAnimation2d;

namespace
{

enum class WandererState
{
  Dormant,
  Active,
  Inactive
};

class BWanderer : public fge::DBehaviour
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
    WandererState m_state = WandererState::Dormant;

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

  if (m_state != WandererState::Active) {
    return;
  }

  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  auto pos = sysGrid.entityPos(m_entityId);
  auto entities = sysGrid.getEntities(pos[0], pos[1]);
  entities.erase(m_entityId);
  m_eventSystem.raiseEvent(EEntityLandOn{m_entityId, pos, entities});

  if (!sysGrid.hasEntity(m_playerId)) {
    m_state = WandererState::Inactive;
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
  if (m_state != WandererState::Active) {
    return;
  }

  if (moved) {
    auto& newPos = sysGrid.entityPos(m_entityId);
    m_eventSystem.raiseEvent(EAttack{m_entityId, sysGrid.getEntities(newPos[0], newPos[1])});
  }
}

void BWanderer::processEvent(const Event& event)
{
  const static HashedString strFadeIn = hashString("fade_in");

  constexpr int sqActivationDist = 36;
  auto& sysGrid = m_ecs.system<SysGrid>();
  auto& sysAnimation = m_ecs.system<SysAnimation2d>();

  if (m_state == WandererState::Active) {
    if (event.name == g_strPlayerMove) {
      if (sysGrid.hasEntity(m_playerId)) {
        auto& playerPos = sysGrid.entityPos(m_playerId);
        auto& pos = sysGrid.entityPos(m_entityId);
        if (playerPos == pos) {
          m_eventSystem.raiseEvent(EAttack{m_entityId, fge::EntityIdSet{ m_playerId }});
        }
      }
    }
    else if (event.name == g_strEntityExplode) {
      auto& e = dynamic_cast<const EEntityExplode&>(event);

      if (e.pos == sysGrid.entityPos(m_entityId)) {
        m_eventSystem.raiseEvent(fge::ERequestDeletion{ m_entityId });
        m_state = WandererState::Inactive;
        return;
      }
    }
  }
  else if (m_state == WandererState::Dormant) {
    if (event.name == g_strPlayerMove) {
      if (sysGrid.hasEntity(m_playerId)) {
        auto& e = dynamic_cast<const EPlayerMove&>(event);
        auto& pos = sysGrid.entityPos(m_entityId);

        auto diff = pos - e.pos;
        int sqDist = diff[0] * diff[0] + diff[1] * diff[1];

        if (sqDist <= sqActivationDist) {
          m_state = WandererState::Active;
          m_ecs.componentStore().component<fge::CRender2d>(m_entityId).visible = true;
          sysAnimation.playAnimation(m_entityId, strFadeIn, [this]() { makeMove(); });
        }
      }
    }
  }
}

} // namespace

fge::DBehaviourPtr createBWanderer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId)
{
  return std::make_unique<BWanderer>(ecs, eventSystem, entityId, playerId);
}

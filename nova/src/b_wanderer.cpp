#include "b_wanderer.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include "sys_grid.hpp"
#include "sys_animation.hpp"
#include "sys_render.hpp"
#include "events.hpp"
#include "time_service.hpp"

namespace
{

class BWanderer : public BehaviourData
{
  public:
    BWanderer(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService, EntityId entityId,
      EntityId playerId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    TimeService& m_timeService;
    EntityId m_entityId;
    EntityId m_playerId;
    bool m_active = false;
};

BWanderer::BWanderer(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, EntityId playerId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_timeService(timeService)
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
    g_strEntityLandOn,
    g_strAnimationFinish
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

  if (event.name == g_strEntityExplode) {
    auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

    auto& e = dynamic_cast<const EEntityExplode&>(event);
    if (sysGrid.hasEntityAt(m_entityId, e.pos[0], e.pos[1])) {
      m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
    }

    return;
  }

  if (m_active) {
    if (event.name == g_strAnimationFinish) {
      auto& e = dynamic_cast<const EAnimationFinish&>(event);

      if (e.animationName == strMoveLeft || e.animationName == strMoveRight ||
        e.animationName == strMoveUp || e.animationName == strMoveDown ||
        e.animationName == strFadeIn) {

        if (!sysGrid.hasEntity(m_playerId)) {
          m_active = false;
          return;
        }

        auto playerPos = sysGrid.entityPos(m_playerId);
        auto pos = sysGrid.entityPos(m_entityId);

        auto diff = pos - playerPos;
        bool moved = false;
        if (abs(diff[0]) > abs(diff[1])) {
          if (diff[0] > 0) {
            if (sysGrid.tryMove(m_entityId, -1, 0)) {
              sysAnimation.playAnimation(m_entityId, strMoveLeft);
              moved = true;
            }
          }
          else if (diff[0] < 0) {
            if (sysGrid.tryMove(m_entityId, 1, 0)) {
              sysAnimation.playAnimation(m_entityId, strMoveRight);
              moved = true;
            }
          }
        }
        else {
          if (diff[1] > 0) {
            if (sysGrid.tryMove(m_entityId, 0, -1)) {
              sysAnimation.playAnimation(m_entityId, strMoveDown);
              moved = true;
            }
          }
          else if (diff[1] < 0) {
            if (sysGrid.tryMove(m_entityId, 0, 1)) {
              sysAnimation.playAnimation(m_entityId, strMoveUp);
              moved = true;
            }
          }
        }

        if (moved) {
          auto makeTask = [&sysGrid](EventSystem& eventSystem, EntityId id) {
            return [&eventSystem, &sysGrid, id]() {
              if (sysGrid.hasEntity(id)) {
                auto pos = sysGrid.entityPos(id);
                auto entities = sysGrid.getEntities(pos[0], pos[1]);
                entities.erase(id);

                auto event = std::make_unique<EEntityLandOn>(id, pos, entities);
                eventSystem.queueEvent(std::move(event));
              }
            };
          };

          m_timeService.scheduleTask(30, makeTask(m_eventSystem, m_entityId));
        }
      }
    }
    else if (event.name == g_strEntityLandOn) {
      auto& e = dynamic_cast<const EEntityLandOn&>(event);
      m_eventSystem.queueEvent(std::make_unique<EAttack>(m_entityId, EntityIdSet{ e.entityId }));
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
        m_ecs.componentStore().component<CRender>(m_entityId).visible = true;
        sysAnimation.playAnimation(m_entityId, strFadeIn);
      }
    }
  }
}

} // namespace

BehaviourDataPtr createBWanderer(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, EntityId playerId)
{
  return std::make_unique<BWanderer>(ecs, eventSystem, timeService, entityId, playerId);
}

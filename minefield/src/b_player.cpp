#include "b_player.hpp"
#include "game_events.hpp"
#include "sys_grid.hpp"
#include <fge/events.hpp>
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

const auto strDie = hashString("die");
const auto strMoveLeft = hashString("move_left");
const auto strMoveRight = hashString("move_right");
const auto strMoveUp = hashString("move_up");
const auto strMoveDown = hashString("move_down");
const auto strDelayDeath = hashString("BPlayerImpl_delay_death");

class BPlayerImpl : public BPlayer
{
  public:
    BPlayerImpl(Ecs& ecs, EventSystem& eventSystem, EntityId entityId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

    bool moveUp() override;
    bool moveDown() override;
    bool moveLeft() override;
    bool moveRight() override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;

    bool move(int dx, int dy, HashedString animation);
    void postMove();
};

BPlayerImpl::BPlayerImpl(Ecs& ecs, EventSystem& eventSystem, EntityId entityId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
{
}

HashedString BPlayerImpl::name() const
{
  static HashedString strUserControl = hashString("player");
  return strUserControl;
}

const std::set<HashedString>& BPlayerImpl::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityExplode,
    g_strAttack,
    g_strTimeout,
    strDelayDeath,
  };
  return subs;
}

void BPlayerImpl::postMove()
{
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

  // We remove ourselves from the grid when we die
  if (!sysGrid.hasEntity(m_entityId)) {
    return;
  }

  auto pos = sysGrid.entityPos(m_entityId);
  auto entities = sysGrid.getEntities(pos[0], pos[1]);
  entities.erase(m_entityId);

  m_eventSystem.raiseEvent(EEntityLandOn{m_entityId, pos, entities});
}

bool BPlayerImpl::move(int dx, int dy, HashedString animation)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(fge::ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

  // We remove ourselves from the grid when we die
  if (!sysGrid.hasEntity(m_entityId)) {
    return false;
  }

  if (sysAnimation.hasAnimationPlaying(m_entityId)) {
    return false;
  }

  if (sysGrid.tryMove(m_entityId, dx, dy)) {
    sysAnimation.playAnimation(m_entityId, animation, [this]() { postMove(); });
    m_eventSystem.raiseEvent(EPlayerMove{sysGrid.entityPos(m_entityId)});

    return true;
  }

  return false;
}

bool BPlayerImpl::moveUp()
{
  return move(0, 1, strMoveUp);
}

bool BPlayerImpl::moveDown()
{
  return move(0, -1, strMoveDown);
}

bool BPlayerImpl::moveRight()
{
  return move(1, 0, strMoveRight);
}

bool BPlayerImpl::moveLeft()
{
  return move(-1, 0, strMoveLeft);
}

void BPlayerImpl::processEvent(const Event& event)
{
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(fge::ANIMATION_SYSTEM));
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));

  if (event.name == g_strEntityExplode) {
    auto& e = dynamic_cast<const EEntityExplode&>(event);

    if (sysGrid.entityPos(m_entityId) == e.pos) {
      m_eventSystem.raiseEvent(EPlayerDeath{});
      m_eventSystem.raiseEvent(fge::ERequestDeletion{m_entityId});
    }
  }
  else if (event.name == g_strAttack) {
    sysGrid.removeEntity(m_entityId);
    m_eventSystem.scheduleEvent(15,
      std::make_unique<Event>(strDelayDeath, fge::EntityIdSet{ m_entityId }));
  }
  else if (event.name == g_strTimeout) {
    sysAnimation.stopAnimation(m_entityId);
    sysAnimation.playAnimation(m_entityId, strDie, [this]() {
      m_eventSystem.raiseEvent(fge::ERequestDeletion{m_entityId});
    });
    m_eventSystem.raiseEvent(EPlayerDeath{});
  }
  else if (event.name == strDelayDeath) {
    m_eventSystem.raiseEvent(EWandererAttack{});

    sysAnimation.queueAnimation(m_entityId, strDie, [this]() {
      m_eventSystem.raiseEvent(fge::ERequestDeletion{m_entityId});
    });

    m_eventSystem.raiseEvent(EPlayerDeath{});
  }
}

} // namespace

BPlayerPtr createBPlayer(Ecs& ecs, EventSystem& eventSystem, EntityId entityId)
{
  return std::make_unique<BPlayerImpl>(ecs, eventSystem, entityId);
}

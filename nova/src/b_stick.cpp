#include "b_stick.hpp"
#include "game_events.hpp"
#include "events.hpp"
#include "sys_grid.hpp"
#include "systems.hpp"

namespace
{

static const auto strThrow = hashString("throw");
static const auto strFade = hashString("fade");

class BStick : public BehaviourData
{
  public:
    BStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId,
      AnimationId throwAnimation);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    EntityId m_playerId;
    AnimationId m_throwAnimation;
    int m_destX = -1;
    int m_destY = -1;

    void throwStick();
    void onLand();
};

BStick::BStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, EntityId playerId,
  AnimationId throwAnimation)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_playerId(playerId)
  , m_throwAnimation(throwAnimation)
{
}

HashedString BStick::name() const
{
  static auto strName = hashString("stick");
  return strName;
}

const std::set<HashedString>& BStick::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityLandOn,
    g_strThrow
  };
  return subs;
}

void BStick::onLand()
{
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  assert(sysGrid.goTo(m_entityId, m_destX, m_destY));

  auto entities = sysGrid.getEntities(m_destX, m_destY);
  entities.erase(m_entityId);

  m_eventSystem.raiseEvent(EEntityLandOn{m_entityId, Vec2i{ m_destX, m_destY }, entities});

  sysAnimation.playAnimation(m_entityId, strFade, [this]() {
    m_eventSystem.raiseEvent(ERequestDeletion{m_entityId});
  });
}

void BStick::throwStick()
{
  auto& sysGrid = dynamic_cast<SysGrid&>(m_ecs.system(GRID_SYSTEM));
  auto& sysAnimation = dynamic_cast<SysAnimation&>(m_ecs.system(ANIMATION_SYSTEM));

  auto& pos = sysGrid.entityPos(m_entityId);

  Vec2f delta{
    static_cast<float_t>(m_destX - pos[0]) * GRID_CELL_W,
    static_cast<float_t>(m_destY - pos[1]) * GRID_CELL_H
  };

  auto anim = std::unique_ptr<Animation>(new Animation{
    .name = strThrow,
    .duration = 30,
    .frames = {
      AnimationFrame{
        .pos = delta,
        .rotation = 360.f,
        .pivot = Vec2f{ 0.f, 0.5f },
        .scale = Vec2f{ 1.f, 1.f },
        .textureRect = std::nullopt,
        .colour = Vec4f{ 1.f, 1.f, 1.f, 1.f }
      }
    }
  });

  sysAnimation.replaceAnimation(m_throwAnimation, std::move(anim));
  sysAnimation.playAnimation(m_entityId, strThrow, [this]() { onLand(); });
}

void BStick::processEvent(const Event& event)
{
  if (event.name == g_strEntityLandOn) {
    auto& e = dynamic_cast<const EEntityLandOn&>(event);

    if (e.entityId == m_playerId) {
      m_eventSystem.raiseEvent(EToggleThrowingMode{m_entityId});
    }
  }
  else if (event.name == g_strThrow) {
    auto& e = dynamic_cast<const EThrow&>(event);

    if (e.stickId == m_entityId) {
      m_destX = e.x;
      m_destY = e.y;

      throwStick();
    }
  }
}

} // namespace

BehaviourDataPtr createBStick(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  EntityId playerId, AnimationId throwAnimation)
{
  return std::make_unique<BStick>(ecs, eventSystem, entityId, playerId, throwAnimation);
}

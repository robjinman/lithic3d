#include "b_numeric_tile.hpp"
#include "sys_render.hpp"
#include "event_system.hpp"
#include "game_events.hpp"
#include "events.hpp"
#include "systems.hpp"

namespace
{

const auto strDelayUpdate = hashString("BNumericTile_delay_update");

struct EDelayUpdate : Event {
  EDelayUpdate(EntityId id, bool makeVisible)
    : Event(strDelayUpdate, { id })
    , makeVisible(makeVisible) {}

  bool makeVisible;
};

class BNumericTile : public BehaviourData
{
  public:
    BNumericTile(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, const Vec2i& pos,
      int value);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& e) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    Vec2i m_pos;
    uint32_t m_value;
};

BNumericTile::BNumericTile(Ecs& ecs, EventSystem& eventSystem, EntityId entityId, const Vec2i& pos,
  int value)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_pos(pos)
  , m_value(value)
{}

HashedString BNumericTile::name() const
{
  static auto strNumericTile = hashString("numeric_tile");
  return strNumericTile;
}

const std::set<HashedString>& BNumericTile::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strEntityExplode,
    strDelayUpdate
  };
  return subs;
}

void BNumericTile::processEvent(const Event& event)
{
  auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

  if (event.name == g_strEntityExplode) {
    auto& e = dynamic_cast<const EEntityExplode&>(event);

    // Assuming all exploding entities are mines

    if (abs(e.pos[0] - m_pos[0]) < 2 && abs(e.pos[1] - m_pos[1]) < 2) {
      m_eventSystem.scheduleEvent(30, std::make_unique<EDelayUpdate>(m_entityId, m_pos == e.pos));
    }
  }
  else if (event.name == strDelayUpdate) {
    auto& e = dynamic_cast<const EDelayUpdate&>(event);

    --m_value;

    if (m_value > 0) {
      Rectf rect{
        .x = pxToUvX(16.f * (m_value - 1)),
        .y = pxToUvY(400.f, 16.f),
        .w = pxToUvW(16.f),
        .h = pxToUvH(16.f)
      };

      sysRender.setTextureRect(m_entityId, rect);
      if (e.makeVisible) {
        sysRender.setVisible(m_entityId, true);
      }
    }
    else {
      m_eventSystem.raiseEvent(ERequestDeletion{m_entityId});
    }
  }
}

} // namespace

BehaviourDataPtr createBNumericTile(Ecs& ecs, EventSystem& eventSystem, EntityId entityId,
  const Vec2i& pos, int value)
{
  return std::make_unique<BNumericTile>(ecs, eventSystem, entityId, pos, value);
}

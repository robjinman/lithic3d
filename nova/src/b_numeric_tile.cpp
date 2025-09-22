#include "b_numeric_tile.hpp"
#include "sys_render.hpp"
#include "event_system.hpp"
#include "game_events.hpp"
#include "events.hpp"
#include "systems.hpp"
#include "time_service.hpp"

namespace
{

class BNumericTile : public BehaviourData
{
  public:
    BNumericTile(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService, EntityId entityId,
      const Vec2i& pos, int value);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& e) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    TimeService& m_timeService;
    EntityId m_entityId;
    Vec2i m_pos;
    uint32_t m_value;
};

BNumericTile::BNumericTile(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, const Vec2i& pos, int value)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_timeService(timeService)
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
    g_strEntityExplode
  };
  return subs;
}

void BNumericTile::processEvent(const Event& e)
{
  if (e.name == g_strEntityExplode) {
    auto& event = dynamic_cast<const EEntityExplode&>(e);

    // Assuming all exploding entities are mines

    auto pos = event.pos;
    if (abs(pos[0] - m_pos[0]) < 2 && abs(pos[1] - m_pos[1]) < 2) {
      m_timeService.scheduleTask(30, [this, pos]() {
        auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));

        --m_value;

        if (m_value > 0) {
          Rectf rect{
            .x = pxToUvX(16.f * (m_value - 1)),
            .y = pxToUvY(400.f, 16.f),
            .w = pxToUvW(16.f),
            .h = pxToUvH(16.f)
          };

          sysRender.setTextureRect(m_entityId, rect);
          if (m_pos == pos) {
            sysRender.setVisible(m_entityId, true);
          }
        }
        else {
          m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
        }
      });
    }
  }
}

} // namespace

BehaviourDataPtr createBNumericTile(Ecs& ecs, EventSystem& eventSystem, TimeService& timeService,
  EntityId entityId, const Vec2i& pos, int value)
{
  return std::make_unique<BNumericTile>(ecs, eventSystem, timeService, entityId, pos, value);
}

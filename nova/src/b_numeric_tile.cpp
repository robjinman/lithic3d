#include "b_numeric_tile.hpp"
#include "sys_render.hpp"
#include "event_system.hpp"
#include "game_events.hpp"

namespace
{

class BNumericTile : public CBehaviour
{
  public:
    BNumericTile(SysRender& sysRender, EventSystem& eventSystem, EntityId entityId,
      const Vec2i& pos, int value)
      : m_sysRender(sysRender)
      , m_eventSystem(eventSystem)
      , m_entityId(entityId)
      , m_pos(pos)
      , m_value(value)
    {}

    HashedString name() const override
    {
      static auto strNumericTile = hashString("numeric_tile");
      return strNumericTile;
    }

    const std::set<HashedString>& subscriptions() const override
    {
      static std::set<HashedString> subs{
        g_strEntityExplode
      };
      return subs;
    }

    void processEvent(const GameEvent& e) override
    {
      if (e.name == g_strEntityExplode) {
        auto& event = dynamic_cast<const EEntityExplode&>(e);

        // Assuming all exploding entities are mines

        if (abs(event.pos[0] - m_pos[0]) < 2 && abs(event.pos[1] - m_pos[1]) < 2) {
          --m_value;

          if (m_value > 0) {
            Rectf rect{
              .x = pxToUvX(16.f * (m_value - 1)),
              .y = pxToUvY(400.f, 16.f),
              .w = pxToUvW(16.f),
              .h = pxToUvH(16.f)
            };

            m_sysRender.setTextureRect(m_entityId, rect);
            if (m_pos == event.pos) {
              m_sysRender.setVisible(m_entityId, true);
            }
          }
          else {
            m_eventSystem.queueEvent(std::make_unique<ERequestDeletion>(m_entityId));
          }
        }
      }
    }

  private:
    SysRender& m_sysRender;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    Vec2i m_pos;
    uint32_t m_value;
};

} // namespace

CBehaviourPtr createBNumericTile(SysRender& sysRender, EventSystem& eventSystem, EntityId entityId,
  const Vec2i& pos, int value)
{
  return std::make_unique<BNumericTile>(sysRender, eventSystem, entityId, pos, value);
}

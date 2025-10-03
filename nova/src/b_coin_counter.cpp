#include "b_coin_counter.hpp"
#include "sys_render.hpp"
#include "game_events.hpp"
#include "systems.hpp"
#include <iomanip>

namespace
{

class BCoinCounter : public BehaviourData
{
  public:
    BCoinCounter(uint32_t coinsRequired, Ecs& ecs, EventSystem& eventSystem, EntityId entityId);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const Event& event) override;

  private:
    Ecs& m_ecs;
    EventSystem& m_eventSystem;
    EntityId m_entityId;
    int m_remaining;
};

BCoinCounter::BCoinCounter(uint32_t coinsRequired, Ecs& ecs, EventSystem& eventSystem,
  EntityId entityId)
  : m_ecs(ecs)
  , m_eventSystem(eventSystem)
  , m_entityId(entityId)
  , m_remaining(coinsRequired)
{
}

HashedString BCoinCounter::name() const
{
  static auto strName = hashString("coin_counter");
  return strName;
}

const std::set<HashedString>& BCoinCounter::subscriptions() const
{
  static std::set<HashedString> subs{
    g_strItemCollect
  };
  return subs;
}

void BCoinCounter::processEvent(const Event& event)
{
  if (event.name == g_strItemCollect && m_remaining > 0) {
    auto& e = dynamic_cast<const EItemCollect&>(event);

    m_remaining = std::max(0, m_remaining - static_cast<int>(e.value));

    std::stringstream ss;
    ss << std::setw(2) << std::setfill('0') << m_remaining;

    auto& sysRender = dynamic_cast<SysRender&>(m_ecs.system(RENDER_SYSTEM));
    sysRender.updateDynamicText(m_entityId, ss.str());

    if (m_remaining == 0) {
      m_eventSystem.queueEvent(std::make_unique<EGoldTargetAttained>());
    }
  }
}

} // namespace

BehaviourDataPtr createBCoinCounter(uint32_t coinsRequired, Ecs& ecs, EventSystem& eventSystem,
  EntityId entityId)
{
  return std::make_unique<BCoinCounter>(coinsRequired, ecs, eventSystem, entityId);
}

#include "sys_ui.hpp"
#include "input_state.hpp"
#include "logger.hpp"
#include "sys_spatial.hpp"

namespace
{

class SysUiImpl : public SysUi
{
  public:
    SysUiImpl(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

    void addEntity(EntityId id, const UiData& data) override;

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    EventSystem& m_eventSystem;
};

SysUiImpl::SysUiImpl(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
  , m_eventSystem(eventSystem)
{}

void SysUiImpl::addEntity(EntityId id, const UiData& data)
{
}

void SysUiImpl::removeEntity(EntityId entityId)
{
}

bool SysUiImpl::hasEntity(EntityId entityId) const
{
  return m_componentStore.hasComponentForEntity<CUi>(entityId);
}

void SysUiImpl::update(Tick tick, const InputState& inputState)
{
  auto& mousePos = inputState.mousePos;
  auto groups = m_componentStore.components<CSpatialFlags, CGlobalTransform, CUi>();

  for (auto& group : groups) {
    auto flags = group.components<CSpatialFlags>();
    auto transforms = group.components<CGlobalTransform>();
    auto uiComps = group.components<CUi>();
    auto n = group.numEntities();

    for (size_t i = 0; i < n; ++i) {
      if (!(flags[i].enabled && flags[i].parentEnabled)) {
        continue;
      }

      auto& t = transforms[i];
      auto& uiComp = uiComps[i];

      Vec2f pos{ t.transform.at(0, 3), t.transform.at(1, 3) };
      Vec2f size{ t.transform.at(0, 0), t.transform.at(1, 1) };

      if (inRange(mousePos[0], pos[0], pos[0] + size[0]) &&
        inRange(mousePos[1], pos[1], pos[1] + size[1])) {

        EntityId id = group.entityIds()[i];

        if (inputState.mouseBtn) {
          uiComp.mouseButtonDown = true;
        }
        else {
          if (uiComp.mouseButtonDown) {
            uiComp.mouseButtonDown = false;
            m_eventSystem.queueEvent(std::make_unique<EUiItemActivate>(id));
          }
        }

        if (!uiComp.mouseOver) {
          m_logger.info(STR("Mouse enter " << group.entityIds()[i]));
          uiComp.mouseOver = true;
        }
      }
      else {
        if (uiComp.mouseOver) {
          m_logger.info(STR("Mouse exit " << group.entityIds()[i]));
          uiComp.mouseOver = false;
        }
      }
    }
  }
}

void SysUiImpl::processEvent(const Event& event)
{

}

} // namespace

SysUiPtr createSysUi(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysUiImpl>(componentStore, eventSystem, logger);
}

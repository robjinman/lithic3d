#include "sys_ui.hpp"

namespace
{

class SysUiImpl : public SysUi
{
  public:
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const GameEvent& event) override;
};

void SysUiImpl::removeEntity(EntityId entityId)
{

}

bool SysUiImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysUiImpl::update(Tick tick)
{

}

void SysUiImpl::processEvent(const GameEvent& event)
{

}

}

SysUiPtr createSysUi()
{
  return std::make_unique<SysUiImpl>();
}

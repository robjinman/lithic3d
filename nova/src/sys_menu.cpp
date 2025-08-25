#include "sys_menu.hpp"
#include "event_system.hpp"

namespace
{

class SysMenuImpl : public SysMenu
{
  public:
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const Event& event) override;
};

void SysMenuImpl::removeEntity(EntityId entityId)
{

}

bool SysMenuImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysMenuImpl::update(Tick tick)
{

}

void SysMenuImpl::processEvent(const Event& event)
{

}

} // namespace

SysMenuPtr createSysMenu()
{
  return std::make_unique<SysMenuImpl>();
}

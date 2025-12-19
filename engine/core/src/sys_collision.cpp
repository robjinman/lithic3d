#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"

namespace lithic3d
{
namespace
{

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DSphere& data) override;
    void addEntity(EntityId id, const DTerrainChunk data) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DSphere& data)
{

}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk data)
{

}

void SysCollisionImpl::removeEntity(EntityId entityId)
{

}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{

}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{

}

void SysCollisionImpl::processEvent(const Event& event)
{

}

} // namespace

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysCollisionImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d

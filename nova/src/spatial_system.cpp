#include "spatial_system.hpp"

namespace
{

class SpatialSystemImpl : public SpatialSystem
{
  public:
    SpatialSystemImpl(Logger& logger);

    void addComponent(ComponentPtr component) override;
    void removeComponent(EntityId entityId) override;
    bool hasComponent(EntityId entityId) const override;
    CSpatial& getComponent(EntityId entityId) override;
    const CSpatial& getComponent(EntityId entityId) const override;
    void update() override;

  private:
    Logger& m_logger;
};

SpatialSystemImpl::SpatialSystemImpl(Logger& logger)
  : m_logger(logger)
{

}

void SpatialSystemImpl::addComponent(ComponentPtr component)
{

}

void SpatialSystemImpl::removeComponent(EntityId entityId)
{

}

bool SpatialSystemImpl::hasComponent(EntityId entityId) const
{

}

CSpatial& SpatialSystemImpl::getComponent(EntityId entityId)
{

}

const CSpatial& SpatialSystemImpl::getComponent(EntityId entityId) const
{

}

void SpatialSystemImpl::update()
{

}

}

SpatialSystemPtr createSpatialSystem(Logger& logger)
{
  return std::make_unique<SpatialSystemImpl>(logger);
}

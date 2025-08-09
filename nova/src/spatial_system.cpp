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
    std::vector<CSpatial> m_data;
    std::vector<uint32_t> m_lookup;   // EntityId -> m_data index
};

SpatialSystemImpl::SpatialSystemImpl(Logger& logger)
  : m_logger(logger)
{

}

void SpatialSystemImpl::addComponent(ComponentPtr component)
{
  auto id = component->id();

  m_data.push_back(*dynamic_cast<CSpatial*>(component.release()));

  if (id + 1 > m_lookup.size()) {
    m_lookup.resize(id + 1, 0);
  }
  m_lookup[id] = m_data.size() - 1;
}

void SpatialSystemImpl::removeComponent(EntityId entityId)
{
  if (m_data.empty()) {
    return;
  }

  auto idx = m_lookup[entityId];
  auto last = m_data.size() - 1;

  auto lastId = m_data[last].id();

  std::swap(m_data[idx], m_data[last]);
  m_data.pop_back();

  m_lookup[entityId] = 0;
  m_lookup[lastId] = idx;
}

bool SpatialSystemImpl::hasComponent(EntityId entityId) const
{
  return m_lookup.size() + 1 > entityId && m_lookup[entityId] != 0;
}

CSpatial& SpatialSystemImpl::getComponent(EntityId entityId)
{
  return m_data[m_lookup[entityId]];
}

const CSpatial& SpatialSystemImpl::getComponent(EntityId entityId) const
{
  return m_data[m_lookup[entityId]];
}

void SpatialSystemImpl::update()
{
  // TODO
}

}

SpatialSystemPtr createSpatialSystem(Logger& logger)
{
  return std::make_unique<SpatialSystemImpl>(logger);
}

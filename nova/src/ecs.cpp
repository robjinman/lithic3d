#include "ecs.hpp"
#include "event_system.hpp"
#include "logger.hpp"
#include "component_store.hpp"
#include "events.hpp"
#include <unordered_map>

namespace
{

class EcsImpl : public Ecs
{
  public:
    explicit EcsImpl(Logger& logger);

    void addSystem(SystemName name, SystemPtr system) override;
    System& system(SystemName name) override;
    const System& system(SystemName name) const override;
    void update(Tick tick) override;
    void processEvent(const Event& event) override;
    void removeEntity(EntityId entityId) override;
    ComponentStore& componentStore() override;
    const ComponentStore& componentStore() const override;

  private:
    Logger& m_logger;
    ComponentStorePtr m_componentStore;
    std::vector<EntityId> m_pendingDeletion;
    std::unordered_map<SystemName, SystemPtr> m_systems;

    void handleEvent(const Event& event);
    void deletePending();
};

EcsImpl::EcsImpl(Logger& logger)
  : m_logger(logger)
{
  m_componentStore = std::make_unique<ComponentStore>();
}

void EcsImpl::addSystem(SystemName name, SystemPtr system)
{
  m_systems.insert({ name, std::move(system) });
}

System& EcsImpl::system(SystemName name)
{
  return *m_systems.at(name);
}

const System& EcsImpl::system(SystemName name) const
{
  return *m_systems.at(name);
}

void EcsImpl::update(Tick tick)
{
  deletePending();

  for (auto& entry : m_systems) {
    entry.second->update(tick);
  }
}

void EcsImpl::processEvent(const Event& event)
{
  handleEvent(event);

  for (auto& entry : m_systems) {
    entry.second->processEvent(event);
  }
}

void EcsImpl::removeEntity(EntityId entityId)
{
  for (auto& entry : m_systems) {
    DBG_LOG(m_logger, STR("Deleting entity " << entityId));

    entry.second->removeEntity(entityId);
  }
  m_componentStore->remove(entityId);
}

ComponentStore& EcsImpl::componentStore()
{
  return *m_componentStore;
}

const ComponentStore& EcsImpl::componentStore() const
{
  return *m_componentStore;
}

void EcsImpl::handleEvent(const Event& event)
{
  if (event.name == g_strRequestDeletion) {
    auto& e = dynamic_cast<const ERequestDeletion&>(event);
    m_pendingDeletion.push_back(e.entityId);
  }
}

void EcsImpl::deletePending()
{
  for (auto id : m_pendingDeletion) {
    DBG_LOG(m_logger, STR("Deleting entity " << id));

    removeEntity(id);
    m_componentStore->remove(id);
  }
  m_pendingDeletion.clear();
}

} // namespace

EcsPtr createEcs(Logger& logger)
{
  return std::make_unique<EcsImpl>(logger);
}

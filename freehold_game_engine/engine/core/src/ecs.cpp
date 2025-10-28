#include "fge/ecs.hpp"
#include "fge/event_system.hpp"
#include "fge/logger.hpp"
#include "fge/component_store.hpp"
#include "fge/events.hpp"
#include <map>

namespace fge
{
namespace
{

class EcsImpl : public Ecs
{
  public:
    explicit EcsImpl(Logger& logger);

    void addSystem(SystemId id, SystemPtr system) override;
    System& getSystem(SystemId id) override;
    const System& getSystem(SystemId id) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;
    void removeEntity(EntityId entityId) override;
    ComponentStore& componentStore() override;
    const ComponentStore& componentStore() const override;

  private:
    Logger& m_logger;
    ComponentStorePtr m_componentStore;
    std::vector<EntityId> m_pendingDeletion;
    std::map<SystemId, SystemPtr> m_systems;

    void handleEvent(const Event& event);
    void deletePending();
};

EcsImpl::EcsImpl(Logger& logger)
  : m_logger(logger)
{
  m_componentStore = std::make_unique<ComponentStore>();
}

void EcsImpl::addSystem(SystemId id, SystemPtr system)
{
  m_systems.insert({ id, std::move(system) });
}

System& EcsImpl::getSystem(SystemId id)
{
  return *m_systems.at(id);
}

const System& EcsImpl::getSystem(SystemId id) const
{
  return *m_systems.at(id);
}

void EcsImpl::update(Tick tick, const InputState& inputState)
{
  deletePending();

  for (auto& entry : m_systems) {
    entry.second->update(tick, inputState);
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
  DBG_LOG(m_logger, STR("Deleting entity " << entityId));

  for (auto& entry : m_systems) {
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
    removeEntity(id);
  }
  m_pendingDeletion.clear();
}

} // namespace

EcsPtr createEcs(Logger& logger)
{
  return std::make_unique<EcsImpl>(logger);
}

} // namespace fge

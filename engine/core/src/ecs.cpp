#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/event_system.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/component_store.hpp"
#include "lithic3d/events.hpp"
#include "lithic3d/trace.hpp"
#include <map>

namespace lithic3d
{
namespace
{

class EcsImpl : public Ecs
{
  public:
    explicit EcsImpl(Logger& logger);

    void addSystem(SystemId id, SystemPtr system) override;
    uint32_t numSystems() const override;
    System& getSystem(SystemId id) override;
    const System& getSystem(SystemId id) const override;
    void update(Tick tick, const InputState& inputState, const std::set<SystemId>& skip) override;
    void processEvent(const Event& event) override;
    void removeEntity(EntityId entityId) override;
    ComponentStore& componentStore() override;
    const ComponentStore& componentStore() const override;
    EntityIdAllocator& idGen() override;

    ~EcsImpl() override;

  private:
    Logger& m_logger;
    EntityIdAllocatorPtr m_entityIdAllocator;
    ComponentStorePtr m_componentStore;
    std::vector<EntityId> m_pendingDeletion;
    std::map<SystemId, SystemPtr> m_systems;

    void handleEvent(const Event& event);
    void deletePending();
};

EcsImpl::EcsImpl(Logger& logger)
  : m_logger(logger)
{
  m_entityIdAllocator = createEntityIdAllocator(NULL_ENTITY_ID); // TODO
  m_componentStore = std::make_unique<ComponentStore>();
}

EntityIdAllocator& EcsImpl::idGen()
{
  return *m_entityIdAllocator;
}

uint32_t EcsImpl::numSystems() const
{
  return m_systems.size();
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

void EcsImpl::update(Tick tick, const InputState& inputState, const std::set<SystemId>& skip)
{
  deletePending();

  for (auto& entry : m_systems) {
    if (!skip.contains(entry.first)) {
      entry.second->update(tick, inputState);
    }
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

  auto& sysSpatial = system<SysSpatial>();
  std::vector<EntityId> descendents;

  if (sysSpatial.hasEntity(entityId)) {
    descendents = sysSpatial.getDescendents(entityId);
  }

#ifndef NDEBUG
  for (auto id : descendents) {
    m_logger.debug(STR("Deleting descendent of entity " << entityId << " with id " << id));
  }
#endif

  for (auto& entry : m_systems) {
    if (entry.first != Systems::Spatial) {
      for (auto descendent : descendents) {
        entry.second->removeEntity(descendent);
      }
    }

    entry.second->removeEntity(entityId);
  }

  for (auto descendent : descendents) {
    m_componentStore->remove(descendent);
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

EcsImpl::~EcsImpl()
{
  DBG_TRACE(m_logger);
}

} // namespace

EcsPtr createEcs(Logger& logger)
{
  return std::make_unique<EcsImpl>(logger);
}

} // namespace lithic3d

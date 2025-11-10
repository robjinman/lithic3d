#include "fge/sys_behaviour.hpp"
#include "fge/event_system.hpp"
#include "fge/sys_spatial.hpp"
#include <vector>
#include <map>
#include <cassert>

namespace fge
{
namespace
{

class SysBehaviourImpl : public SysBehaviour
{
  public:
    SysBehaviourImpl(ComponentStore& componentStore)
      : m_componentStore(componentStore) {}

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick, const InputState&) override {}
    void processEvent(const Event& event) override;

    void addBehaviour(EntityId entityId, DBehaviourPtr behaviour) override;
    DBehaviour& getBehaviour(EntityId entityId, HashedString name) override;
    const DBehaviour& getBehaviour(EntityId entityId, HashedString name) const override;

  private:
    ComponentStore& m_componentStore;
    std::map<EntityId, std::map<HashedString, DBehaviourPtr>> m_behaviours;
    std::map<HashedString, std::set<EntityId>> m_subscriptions;
};

void SysBehaviourImpl::addBehaviour(EntityId entityId, DBehaviourPtr behaviour)
{
  auto name = behaviour->name();

  for (auto sub : behaviour->subscriptions()) {
    m_subscriptions[sub].insert(entityId);
  }

  m_behaviours[entityId].insert({ name, std::move(behaviour) });
}

DBehaviour& SysBehaviourImpl::getBehaviour(EntityId entityId, HashedString name)
{
  return *m_behaviours.at(entityId).at(name);
}

const DBehaviour& SysBehaviourImpl::getBehaviour(EntityId entityId, HashedString name) const
{
  return *m_behaviours.at(entityId).at(name);
}

void SysBehaviourImpl::removeEntity(EntityId entityId)
{
  m_behaviours.erase(entityId);

  // TODO: Slow?
  for (auto& entry : m_subscriptions) {
    entry.second.erase(entityId);
  }
}

bool SysBehaviourImpl::hasEntity(EntityId entityId) const
{
  return m_behaviours.contains(entityId);
}

void SysBehaviourImpl::processEvent(const Event& event)
{
  // TODO: Don't send events to inactive entities

  // If the event isn't targeted, send to all subscribers
  if (!event.targets.has_value()) {
    auto i = m_subscriptions.find(event.name);
    if (i != m_subscriptions.end()) {
      for (auto id : i->second) {
        if (m_componentStore.component<CSpatialFlags>(id).enabled) {
          auto& behaviours = m_behaviours.at(id);
          for (auto& entry : behaviours) {
            entry.second->processEvent(event);
          }
        }
      }
    }
  }
  // If the event is targeted, send to all targets regardless of whether they subscribe
  // TODO: Consider only sending to subscribers?
  else {
    for (auto id : event.targets.value()) {
      auto i = m_behaviours.find(id);
      if (i != m_behaviours.end()) {
        if (m_componentStore.component<CSpatialFlags>(id).enabled) {
          auto& behaviours = i->second;
          for (auto& entry : behaviours) {
            entry.second->processEvent(event);
          }
        }
      }
    }
  }
}

} // namespace

SysBehaviourPtr createSysBehaviour(ComponentStore& componentStore)
{
  return std::make_unique<SysBehaviourImpl>(componentStore);
}

} // namespace fge

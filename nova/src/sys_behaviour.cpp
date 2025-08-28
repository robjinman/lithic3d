#include "sys_behaviour.hpp"
#include "event_system.hpp"
#include <vector>
#include <map>
#include <cassert>

namespace
{

class SysBehaviourImpl : public SysBehaviour
{
  public:
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick) override {}
    void processEvent(const Event& event) override;

    void addBehaviour(EntityId entityId, CBehaviourPtr behaviour) override;
    CBehaviour& getBehaviour(EntityId entityId, HashedString name) override;
    const CBehaviour& getBehaviour(EntityId entityId, HashedString name) const override;

  private:
    std::map<EntityId, std::map<HashedString, CBehaviourPtr>> m_behaviours;
    std::map<HashedString, std::set<EntityId>> m_subscriptions;
};

void SysBehaviourImpl::addBehaviour(EntityId entityId, CBehaviourPtr behaviour)
{
  auto name = behaviour->name();

  for (auto sub : behaviour->subscriptions()) {
    m_subscriptions[sub].insert(entityId);
  }

  m_behaviours[entityId].insert({ name, std::move(behaviour) });
}

CBehaviour& SysBehaviourImpl::getBehaviour(EntityId entityId, HashedString name)
{
  return *m_behaviours.at(entityId).at(name);
}

const CBehaviour& SysBehaviourImpl::getBehaviour(EntityId entityId, HashedString name) const
{
  return *m_behaviours.at(entityId).at(name);
}

void SysBehaviourImpl::removeEntity(EntityId entityId)
{
  auto i = m_behaviours.find(entityId);
  if (i == m_behaviours.end()) {
    return;
  }
  m_behaviours.erase(i);
}

bool SysBehaviourImpl::hasEntity(EntityId entityId) const
{
  return m_behaviours.contains(entityId);
}

void SysBehaviourImpl::processEvent(const Event& event)
{
  // TODO: Don't send events to inactive entities

  if (event.targets.empty()) {
    auto i = m_subscriptions.find(event.name);
    if (i != m_subscriptions.end()) {
      for (auto id : i->second) {
        auto& behaviours = m_behaviours.at(id);
        for (auto& entry : behaviours) {

          entry.second->processEvent(event);
        }
      }
    }
  }
  else {
    for (auto id : event.targets) {
      auto i = m_behaviours.find(id);
      if (i != m_behaviours.end()) {
        auto& behaviours = i->second;
        for (auto& entry : behaviours) {
          entry.second->processEvent(event);
        }
      }
    }
  }
}

} // namespace

SysBehaviourPtr createSysBehaviour()
{
  return std::make_unique<SysBehaviourImpl>();
}

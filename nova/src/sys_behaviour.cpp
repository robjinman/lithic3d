#include "sys_behaviour.hpp"
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
    void update(const InputState& inputState) override;
    void processEvent(const GameEvent& event) override;

    void addBehaviour(EntityId entityId, CBehaviourPtr behaviour) override;

  private:
    // TODO: Support multiple behaviours per entity?
    std::vector<CBehaviourPtr> m_data;
    std::vector<EntityId> m_ids;      // m_data index -> EntityId
    std::vector<uint32_t> m_lookup;   // EntityId -> m_data index

    std::map<HashedString, std::set<EntityId>> m_subscriptions;
};

void SysBehaviourImpl::addBehaviour(EntityId entityId, CBehaviourPtr behaviour)
{
  for (auto sub : behaviour->subscriptions()) {
    m_subscriptions[sub].insert(entityId);
  }
  m_data.push_back(std::move(behaviour));

  m_ids.push_back(entityId);

  assert(m_data.size() == m_ids.size());

  if (entityId + 1 > m_lookup.size()) {
    m_lookup.resize(entityId + 1, 0);
  }
  m_lookup[entityId] = m_data.size() - 1;
}

void SysBehaviourImpl::removeEntity(EntityId entityId)
{
  if (m_data.empty()) {
    return;
  }

  auto idx = m_lookup[entityId];
  auto& subs = m_data[idx]->subscriptions();
  for (auto sub : subs) {
    m_subscriptions[sub].erase(entityId);
  }

  auto last = m_data.size() - 1;
  auto lastId = m_ids[last];

  std::swap(m_data[idx], m_data[last]);
  std::swap(m_ids[idx], m_ids[idx]);
  m_data.pop_back();
  m_ids.pop_back();

  assert(m_data.size() == m_ids.size());

  m_lookup[entityId] = 0;
  m_lookup[lastId] = idx;
}

bool SysBehaviourImpl::hasEntity(EntityId entityId) const
{
  return m_lookup.size() + 1 > entityId && m_lookup[entityId] != 0;
}

void SysBehaviourImpl::update(const InputState& inputState)
{
  for (auto& behaviour : m_data) {
    behaviour->update(inputState);
  }
}

void SysBehaviourImpl::processEvent(const GameEvent& event)
{
  if (event.targets.empty()) {
    auto i = m_subscriptions.find(event.name);
    if (i != m_subscriptions.end()) {
      for (auto id : i->second) {
        m_data[m_lookup[id]]->processEvent(event);
      }
    }
  }
  else {
    for (auto id : event.targets) {
      m_data[m_lookup[id]]->processEvent(event);
    }
  }
}

}

SysBehaviourPtr createSysBehaviour()
{
  return std::make_unique<SysBehaviourImpl>();
}

#include "sys_behaviour.hpp"

namespace
{

class SysBehaviourImpl : public SysBehaviour
{
  public:
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update() override;
    void processEvent(const GameEvent& event) override;

    void addBehaviour(EntityId entityId, CBehaviourPtr behaviour) override;
};

void SysBehaviourImpl::addBehaviour(EntityId entityId, CBehaviourPtr behaviour)
{

}

void SysBehaviourImpl::removeEntity(EntityId entityId)
{

}

bool SysBehaviourImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysBehaviourImpl::update()
{

}

void SysBehaviourImpl::processEvent(const GameEvent& event)
{

}

}

SysBehaviourPtr createSysBehaviour()
{
  return std::make_unique<SysBehaviourImpl>();
}

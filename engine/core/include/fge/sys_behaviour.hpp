#pragma once

#include "ecs.hpp"

namespace fge
{

class BehaviourData
{
  public:
    virtual HashedString name() const = 0;
    virtual const std::set<HashedString>& subscriptions() const = 0;
    virtual void processEvent(const Event& event) = 0;

    virtual ~BehaviourData() = default;
};

using BehaviourDataPtr = std::unique_ptr<BehaviourData>;

class SysBehaviour : public System
{
  public:
    virtual void addBehaviour(EntityId entityId, BehaviourDataPtr behaviour) = 0;
    virtual BehaviourData& getBehaviour(EntityId entityId, HashedString name) = 0;
    virtual const BehaviourData& getBehaviour(EntityId entityId, HashedString name) const = 0;
};

using SysBehaviourPtr = std::unique_ptr<SysBehaviour>;

class ComponentStore;

SysBehaviourPtr createSysBehaviour(ComponentStore& componentStore);

} // namespace fge

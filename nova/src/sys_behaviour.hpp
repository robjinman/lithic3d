#pragma once

#include "system.hpp"

class CBehaviour
{
  public:
    virtual HashedString name() const = 0;
    virtual const std::set<HashedString>& subscriptions() const = 0;
    virtual void processEvent(const GameEvent& event) = 0;
    virtual void update(Tick tick, const InputState& inputState) = 0;

    virtual ~CBehaviour() {}
};

using CBehaviourPtr = std::unique_ptr<CBehaviour>;

class SysBehaviour : public System
{
  public:
    virtual void addBehaviour(EntityId entityId, CBehaviourPtr behaviour) = 0;
    virtual CBehaviour& getBehaviour(EntityId entityId, HashedString name) = 0;
    virtual const CBehaviour& getBehaviour(EntityId entityId, HashedString name) const = 0;
};

using SysBehaviourPtr = std::unique_ptr<SysBehaviour>;

SysBehaviourPtr createSysBehaviour();

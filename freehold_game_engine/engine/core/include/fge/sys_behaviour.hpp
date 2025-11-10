#pragma once

#include "ecs.hpp"
#include "systems.hpp"

namespace fge
{

class DBehaviour
{
  public:
    virtual HashedString name() const = 0;
    virtual const std::set<HashedString>& subscriptions() const = 0;
    virtual void processEvent(const Event& event) = 0;

    virtual ~DBehaviour() = default;
};

using DBehaviourPtr = std::unique_ptr<DBehaviour>;

class SysBehaviour : public System
{
  public:
    virtual void addBehaviour(EntityId entityId, DBehaviourPtr behaviour) = 0;
    virtual DBehaviour& getBehaviour(EntityId entityId, HashedString name) = 0;
    virtual const DBehaviour& getBehaviour(EntityId entityId, HashedString name) const = 0;

    virtual ~SysBehaviour() = default;

    static const SystemId id = BEHAVIOUR_SYSTEM;
};

using SysBehaviourPtr = std::unique_ptr<SysBehaviour>;

class ComponentStore;

SysBehaviourPtr createSysBehaviour(ComponentStore& componentStore);

} // namespace fge

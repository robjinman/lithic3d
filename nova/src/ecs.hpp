#pragma once

#include "utils.hpp"
#include "units.hpp"
#include "component_store.hpp"
#include <set>
#include <memory>

using SystemName = HashedString;

using EntityIdSet = std::set<EntityId>;

class Event;

class System
{
  public:
    virtual void removeEntity(EntityId entityId) = 0;
    virtual bool hasEntity(EntityId entityId) const = 0;
    virtual void update(Tick tick) = 0;
    virtual void processEvent(const Event& event) = 0;

    virtual ~System() = default;
};

using SystemPtr = std::unique_ptr<System>;

class Ecs
{
  public:
    virtual void addSystem(SystemName name, SystemPtr system) = 0;
    virtual System& system(SystemName name) = 0;
    virtual const System& system(SystemName name) const = 0;
    virtual void update(Tick tick) = 0;
    virtual void processEvent(const Event& event) = 0;
    // Warning: This will immediately delete the entity. Consider firing ERequestDeletion instead.
    virtual void removeEntity(EntityId entityId) = 0;
    virtual ComponentStore& componentStore() = 0;
    virtual const ComponentStore& componentStore() const = 0;

    virtual ~Ecs() = default;
};

using EcsPtr = std::unique_ptr<Ecs>;

class Logger;

EcsPtr createEcs(Logger& logger);

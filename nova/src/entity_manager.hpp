#pragma once

#include "entity.hpp"
#include <functional>

class EntityManager
{
  public:
    virtual Entity& getEntity(EntityId id) = 0;
    virtual void addEntity(EntityPtr entity) = 0;
    virtual void deleteEntity(EntityId id) = 0;

    virtual void update() = 0;
    virtual void forEach(std::function<void(Entity&)>& fn);

    virtual void notify(EntityId entity, const Event& event) = 0;
    virtual void notify(const std::set<EntityId>&, const Event& event) = 0;
    virtual void notifyAll(const Event& event) = 0;

    virtual ~EntityManager() {}
};

using EntityManagerPtr = std::unique_ptr<EntityManager>;

EntityManagerPtr createEntityManager();

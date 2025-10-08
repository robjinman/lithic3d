#pragma once

#include "ecs.hpp"
#include "event_system.hpp"

const HashedString g_strRequestDeletion = hashString("request_deletion");

class ERequestDeletion : public Event
{
  public:
    ERequestDeletion(EntityId entityId)
      : Event(g_strRequestDeletion)
      , entityId(entityId) {}

    ERequestDeletion(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strRequestDeletion, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

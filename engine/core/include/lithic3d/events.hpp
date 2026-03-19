#pragma once

#include "entity_id.hpp"
#include "event_system.hpp"

namespace lithic3d
{

const HashedString g_strRequestDeletion = hashString("request_deletion");
const HashedString g_strWindowResize = hashString("window_resize");

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

class EWindowResize : public Event
{
  public:
    EWindowResize(uint32_t w, uint32_t h)
      : Event(g_strWindowResize)
      , width(w)
      , height(h) {}

    uint32_t width;
    uint32_t height;
};


} // namespace lithic3d

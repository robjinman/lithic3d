#include "lithic3d/entity_id.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/strings.hpp"

namespace lithic3d
{
namespace
{

class EntityIdAllocatorImpl : public EntityIdAllocator
{
  public:
    EntityIdAllocatorImpl(EntityId lastId);

    EntityId getNewEntityId() override;
    EntityId getNewEntityId(const std::string& name) override;
    EntityId namedEntity(const std::string& name) const override;

  private:
    EntityId m_nextId;
};

EntityIdAllocatorImpl::EntityIdAllocatorImpl(EntityId lastId)
  : m_nextId(lastId + 1)
{}

EntityId EntityIdAllocatorImpl::getNewEntityId()
{
  return m_nextId++;
}

EntityId EntityIdAllocatorImpl::getNewEntityId(const std::string& name)
{
  // TODO: Prevent collisions
  return hashString(name);
}

EntityId EntityIdAllocatorImpl::namedEntity(const std::string& name) const
{
  return hashString(name);
}

} // namespace

EntityIdAllocatorPtr createEntityIdAllocator(EntityId lastId)
{
  return std::make_unique<EntityIdAllocatorImpl>(lastId);
}

} // namespace lithic3d

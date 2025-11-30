#include "lithic3d/entity_id.hpp"
#include "lithic3d/exception.hpp"

namespace lithic3d
{
namespace
{

class EntityIdAllocatorImpl : public EntityIdAllocator
{
  public:
    EntityIdAllocatorImpl(EntityId lastId);

    EntityId getNewEntityId() override;

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

} // namespace

EntityIdAllocatorPtr createEntityIdAllocator(EntityId lastId)
{
  return std::make_unique<EntityIdAllocatorImpl>(lastId);
}

} // namespace lithic3d

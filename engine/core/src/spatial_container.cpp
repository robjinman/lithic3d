#include "lithic3d/spatial_container.hpp"

namespace lithic3d
{
namespace {

class SpatialContainerImpl : public SpatialContainer
{
  public:
    void insert(EntityId entityId, const Aabb& aabb) override;
    void move(EntityId entityId, const Aabb& aabb) override;
    void remove(EntityId entityId) override;
    void getIntersecting(const Frustum& volume, std::vector<EntityId>& entities) const override;
};

void SpatialContainerImpl::insert(EntityId entityId, const Aabb& aabb)
{

}

void SpatialContainerImpl::move(EntityId entityId, const Aabb& aabb)
{

}

void SpatialContainerImpl::remove(EntityId entityId)
{

}

void SpatialContainerImpl::getIntersecting(const Frustum& volume,
  std::vector<EntityId>& entities) const
{

}

} // namespace

SpatialContainerPtr createSpatialContainer()
{
  return std::make_unique<SpatialContainerImpl>();
}

}

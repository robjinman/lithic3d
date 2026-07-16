#pragma once

#include "ecs.hpp"
#include "component_types.hpp"
#include "event_system.hpp"
#include "systems.hpp"
#include "loose_octree.hpp"
#include "utils.hpp"
#include <bitset>

namespace lithic3d
{

const HashedString g_strEntityEnable = hashString("entity_enable");

class EEntityEnable : public Event
{
  public:
    EEntityEnable(EntityId entityId, const EntityIdSet& targets)
      : Event(g_strEntityEnable, targets)
      , entityId(entityId) {}

    std::string toString() const override
    {
      return STR(Event::toString() << " ("
        << "entityId = " << entityId << ")");
    }

    EntityId entityId;
};

struct Aabb
{
  Vec3f min;
  Vec3f max;

  inline void add(const Aabb& other)
  {
    min = {
      std::min(min[0], other.min[0]),
      std::min(min[1], other.min[1]),
      std::min(min[2], other.min[2])
    };
    max = {
      std::max(max[0], other.max[0]),
      std::max(max[1], other.max[1]),
      std::max(max[2], other.max[2])
    };
  }
};

Aabb transformAabb(const Aabb& aabb, const Mat4x4f& m);

struct CLocalTransform
{
  // TODO
  //Vec3f translation;
  //Vec3f scale;
  //Mat3x3f rotation; // TODO: Replace with quaternion

  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentTypeId TypeId = CLocalTransformTypeId;
};

static_assert(std::is_trivially_copyable_v<CLocalTransform>);

struct CGlobalTransform
{
  Mat4x4f transform = identityMatrix<4>();

  static constexpr ComponentTypeId TypeId = CGlobalTransformTypeId;
};

static_assert(std::is_trivially_copyable_v<CGlobalTransform>);

namespace SpatialFlags
{
  enum : uint64_t
  {
    Enabled,
    ParentEnabled,
    Dirty,            // Whether the entity has been moved
    Visible0,         // Whether the entity is within the main view frustum
    Visible1,         // Whether the entity is within the shadow pass frustums
    Visible2,
    Visible3
  };
}

struct CSpatialFlags
{
  std::bitset<32> prevFlags{
    bitflag(SpatialFlags::Enabled) |
    bitflag(SpatialFlags::ParentEnabled)
  };
  std::bitset<32> flags{
    bitflag(SpatialFlags::Enabled) |
    bitflag(SpatialFlags::ParentEnabled)
  };

  static constexpr ComponentTypeId TypeId = CSpatialFlagsTypeId;
};

static_assert(std::is_trivially_copyable_v<CSpatialFlags>);

struct CBoundingBox
{
  Aabb modelSpaceAabb;
  Aabb worldSpaceAabb;

  static constexpr ComponentTypeId TypeId = CBoundingBoxTypeId;
};

static_assert(std::is_trivially_copyable_v<CBoundingBox>);

struct DSpatial
{
  using RequiredComponents = type_list<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox
  >;

  Mat4x4f transform = identityMatrix<4>();
  EntityId parent = NULL_ENTITY_ID;
  bool enabled = true;
  Aabb aabb; // Model-space bounding box
};

namespace SysSpatialSubcomponent
{
  enum : uint32_t
  {
    Transform,
    Aabb
  };
}

using SpatialContainer = LooseOctree<EntityId, uint32_t>;
using SpatialContainerPtr = std::unique_ptr<SpatialContainer>;

class SysSpatial : public System
{
  public:
    using System::addEntity;  // Silence clang warning -Woverloaded-virtual

    virtual EntityId root() const = 0;
    virtual std::vector<EntityId> getDescendents(EntityId entityId) const = 0;
    virtual std::vector<EntityId> getChildren(EntityId entityId) const = 0;
    virtual void addEntity(EntityId id, const DSpatial& data) = 0;
    virtual void setEnabled(EntityId id, bool enabled) = 0;

    // TODO: Rethink terminology
    virtual const Mat4x4f& getLocalTransform(EntityId id) const = 0;
    virtual const Mat4x4f& getGlobalTransform(EntityId id) const = 0;

    virtual const Aabb& getAabb(EntityId entityId) const = 0;

    // Relative to self axes
    virtual void translateEntitySelf(EntityId id, const Vec3f& t) = 0;

    // Relative to parent axes
    virtual void rotateEntityLocal(EntityId id, const Mat3x3f& rot) = 0;
    virtual void translateEntityLocal(EntityId id, const Vec3f& t) = 0;
    virtual void setLocalTransform(EntityId id, const Mat4x4f& m) = 0;

    virtual void updateMainFrustum(const Frustum& main) = 0;

    virtual void updateShadowFrustums(const Frustum& shadow0, const Frustum& shadow1,
      const Frustum& shadow2) = 0;

    //virtual EntityIdSet getIntersecting(const Frustum& frustum) const = 0;
    virtual EntityIdSet getIntersecting(const Vec3f& rayStart, const Vec3f& rayEnd) const = 0;

    virtual ~SysSpatial() = default;

    virtual const SpatialContainer& dbg_getOctree() const = 0;

    static const SystemId id = Systems::Spatial;
};

using SysSpatialPtr = std::unique_ptr<SysSpatial>;

SysSpatialPtr createSysSpatial(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

} // namespace lithic3d

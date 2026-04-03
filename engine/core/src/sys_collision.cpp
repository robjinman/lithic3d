#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/input.hpp"
#include <iostream> // TODO

namespace lithic3d
{

std::array<Vec3f, 3> HeightMapSampler::triangle(const Vec2f& p) const
{
  DBG_ASSERT(inRange(p), "Value out of range");

  float xIdx = (p[0] - m_pos[0]) * (m_map.widthPx - 1) / m_map.width;
  float zIdx = (p[1] - m_pos[2]) * (m_map.heightPx - 1) / m_map.height;

  float dx = m_map.width / (m_map.widthPx - 1);
  float dz = m_map.height / (m_map.heightPx - 1);

  auto xIdx0 = floorf(xIdx);
  auto zIdx0 = floorf(zIdx);
  auto xIdx1 = ceilf(xIdx);
  auto zIdx1 = ceilf(zIdx);

  if (xIdx0 == xIdx1) {
    ++xIdx1;
  }
  if (zIdx0 == zIdx1) {
    ++zIdx1;
  }

  auto getVertex = [this, dx, dz](float xIdx, float zIdx) {
    size_t i = static_cast<size_t>(zIdx) * m_map.widthPx + static_cast<size_t>(xIdx);
    return m_pos + Vec3f{ dx * xIdx, m_pos[1] + m_map.data.at(i), dz * zIdx };
  };

  Vec3f A = getVertex(xIdx0, zIdx1);
  Vec3f B = getVertex(xIdx1, zIdx1);
  Vec3f C = getVertex(xIdx1, zIdx0);
  Vec3f D = getVertex(xIdx0, zIdx0);

  Vec2f idx = Vec2f{ xIdx, zIdx };
  float distanceFromD = (idx - Vec2f{ xIdx0, zIdx0 }).squareMagnitude();
  float distanceFromB = (idx - Vec2f{ xIdx1, zIdx1 }).squareMagnitude();

  return distanceFromD < distanceFromB ?
    std::array<Vec3f, 3>{ A, C, D } :
    std::array<Vec3f, 3>{ A, B, C };
}

void HeightMapSampler::triangles(const Vec2f& min, const Vec2f& max,
  std::vector<std::array<Vec3f, 3>>& triangles) const
{
  // TODO: Clip to range?
  if (!inRange(min) || !inRange(max)) {
    return;
  }

  size_t w = m_map.widthPx - 1;
  size_t h = m_map.heightPx - 1;
  auto xIdx0 = static_cast<size_t>(floorf((min[0] - m_pos[0]) * w / m_map.width));
  auto zIdx0 = static_cast<size_t>(floorf((min[1] - m_pos[2]) * h / m_map.height));
  auto xIdx1 = static_cast<size_t>(ceilf((max[0] - m_pos[0]) * w / m_map.width));
  auto zIdx1 = static_cast<size_t>(ceilf((max[1] - m_pos[2]) * h / m_map.height));

  if (xIdx0 == xIdx1) {
    ++xIdx1;
  }
  if (zIdx0 == zIdx1) {
    ++zIdx1;
  }

  DBG_ASSERT(xIdx0 < xIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");
  DBG_ASSERT(zIdx0 < zIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");

  float dx = m_map.width / w;
  float dz = m_map.height / h;

  auto getVertex = [this, dx, dz](size_t xIdx, size_t zIdx) {
    size_t i = zIdx * m_map.widthPx + xIdx;
    return m_pos + Vec3f{ dx * xIdx, m_pos[1] + m_map.data.at(i), dz * zIdx };
  };

  for (size_t j = zIdx0; j < zIdx1; ++j) {
    for (size_t i = xIdx0; i < xIdx1; ++i) {
      Vec3f A = getVertex(i, j + 1);
      Vec3f B = getVertex(i + 1, j + 1);
      Vec3f C = getVertex(i + 1, j);
      Vec3f D = getVertex(i, j);

      triangles.push_back({ A, C, D });
      triangles.push_back({ A, B, C });
    }
  }
}

void HeightMapSampler::vertices(const Vec2f& min, const Vec2f& max,
  std::vector<Vec3f>& vertices) const
{
  // TODO: Clip to range?
  if (!inRange(min) || !inRange(max)) {
    return;
  }

  size_t w = m_map.widthPx - 1;
  size_t h = m_map.heightPx - 1;
  auto xIdx0 = static_cast<size_t>(floorf((min[0] - m_pos[0]) * w / m_map.width));
  auto zIdx0 = static_cast<size_t>(floorf((min[1] - m_pos[2]) * h / m_map.height));
  auto xIdx1 = static_cast<size_t>(ceilf((max[0] - m_pos[0]) * w / m_map.width));
  auto zIdx1 = static_cast<size_t>(ceilf((max[1] - m_pos[2]) * h / m_map.height));

  if (xIdx0 == xIdx1) {
    ++xIdx1;
  }
  if (zIdx0 == zIdx1) {
    ++zIdx1;
  }

  DBG_ASSERT(xIdx0 < xIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");
  DBG_ASSERT(zIdx0 < zIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");

  float dx = m_map.width / w;
  float dz = m_map.height / h;

  auto getVertex = [this, dx, dz](size_t xIdx, size_t zIdx) {
    size_t i = zIdx * m_map.widthPx + xIdx;
    return m_pos + Vec3f{ dx * xIdx, m_pos[1] + m_map.data.at(i), dz * zIdx };
  };

  for (size_t j = zIdx0; j <= zIdx1; ++j) {
    for (size_t i = xIdx0; i <= xIdx1; ++i) {
      vertices.push_back(getVertex(i, j));
    }
  }
}

void HeightMapSampler::edges(const Vec2f& min, const Vec2f& max, std::vector<Edge>& edges) const
{
  // TODO: Clip to range?
  if (!inRange(min) || !inRange(max)) {
    return;
  }

  size_t w = m_map.widthPx - 1;
  size_t h = m_map.heightPx - 1;
  auto xIdx0 = static_cast<size_t>(floorf((min[0] - m_pos[0]) * w / m_map.width));
  auto zIdx0 = static_cast<size_t>(floorf((min[1] - m_pos[2]) * h / m_map.height));
  auto xIdx1 = static_cast<size_t>(ceilf((max[0] - m_pos[0]) * w / m_map.width));
  auto zIdx1 = static_cast<size_t>(ceilf((max[1] - m_pos[2]) * h / m_map.height));

  if (xIdx0 == xIdx1) {
    ++xIdx1;
  }
  if (zIdx0 == zIdx1) {
    ++zIdx1;
  }

  DBG_ASSERT(xIdx0 < xIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");
  DBG_ASSERT(zIdx0 < zIdx1, "Bad min-max bounds: (" << min << "), (" << max << ")");

  float dx = m_map.width / w;
  float dz = m_map.height / h;

  auto getVertex = [this, dx, dz](size_t xIdx, size_t zIdx) {
    size_t i = zIdx * m_map.widthPx + xIdx;
    return Vec3f{ dx * xIdx, m_pos[1] + m_map.data.at(i), dz * zIdx };
  };

  for (size_t j = zIdx0; j < zIdx1; ++j) {
    for (size_t i = xIdx0; i < xIdx1; ++i) {
      Vec3f A = getVertex(i, j + 1);
      Vec3f B = getVertex(i + 1, j + 1);
      Vec3f C = getVertex(i + 1, j);
      Vec3f D = getVertex(i, j);

      if (i > xIdx0) {
        edges.push_back({ A, D });
      }
      if (j > zIdx0) {
        edges.push_back({ C, D });
      }
      edges.push_back({ A, B });
      edges.push_back({ B, C });
      edges.push_back({ A, C });
    }
  }
}

namespace
{

const float G = -metresToWorldUnits(9.8f) / (TICKS_PER_SECOND * TICKS_PER_SECOND);

uint32_t findFreeIndex(const std::array<Force, MAX_FORCES>& array)
{
  for (size_t i = 0; i < MAX_FORCES; ++i) {
    if (array[i].lifetime == 0) {
      return i;
    }
  }
  return MAX_FORCES;
}

// Returns the matrix that transforms FROM the new space
Mat3x3f changeOfBasisMatrix(const Vec3f& yAxis, const Vec3f& other)
{
  Vec3f xAxis = yAxis.cross(other).normalise();
  Vec3f zAxis = yAxis.cross(xAxis).normalise();

  return {
    xAxis[0], yAxis[0], zAxis[0],
    xAxis[1], yAxis[1], zAxis[1],
    xAxis[2], yAxis[2], zAxis[2]
  };
}

Mat3x3f skewSymmetricMatrix(const Vec3f& v)
{
  return {
    0.f, -v[2], v[1],
    v[2], 0.f, -v[0],
    -v[1], v[0], 0.f
  };
}

Mat3x3f computeInverseInertialTensor(const BoundingBox& box, float inverseMass)
{
  float w = box.max[0] - box.min[0];
  float h = box.max[1] - box.min[1];
  float d = box.max[2] - box.min[2];

  return {
    12.f * inverseMass / (h * h + d * d), 0.f, 0.f,
    0.f, 12.f * inverseMass / (w * w + d * d), 0.f,
    0.f, 0.f, 12.f * inverseMass / (w * w + h * h)
  };
}

Mat3x3f transformTensor(const Mat3x3f& I, const Mat4x4f& m)
{
  // Normalise to remove scale
  Vec3f i = Vec3f{ m.at(0, 0), m.at(1, 0), m.at(2, 0) }.normalise();
  Vec3f j = Vec3f{ m.at(0, 1), m.at(1, 1), m.at(2, 1) }.normalise();
  Vec3f k = Vec3f{ m.at(0, 2), m.at(1, 2), m.at(2, 2) }.normalise();

  Mat3x3f R{
    i[0], j[0], k[0],
    i[1], j[1], k[1],
    i[2], j[2], k[2]
  };

  Mat3x3f invR{
    i[0], i[1], i[2],
    j[0], j[1], j[2],
    k[0], k[1], k[2]
  };

  return R * I * invR;
}

struct ObjectComponents
{
  EntityId entityId = 0;
  CLocalTransform* localTransform = nullptr;
  CGlobalTransform* globalTransform = nullptr;
  CSpatialFlags* spatialFlags = nullptr;
  CCollision* collision = nullptr;
  CCollisionDynamic* dynamic = nullptr;
  CCollisionRotational* rotational = nullptr;
  CCollisionBox* box = nullptr;
  CCollisionCapsule* capsule = nullptr;
  CCollisionTerrain* terrain = nullptr;
};

struct CollisionPair
{
  ObjectComponents A;
  ObjectComponents B;
};

struct Contact
{
  ObjectComponents A;
  ObjectComponents B;
  Vec3f point;
  Vec3f normal;
  float penetration = 0.f;
  Mat3x3f toContactSpace;
  Mat3x3f fromContactSpace;
};

struct PolyhedronData
{
  Vec3f centreOfMass;
  std::vector<Vec3f> vertices;
  std::vector<std::array<uint16_t, 2>> edges;
  std::vector<std::array<uint16_t, 3>> faces;
};

using PolyhedronDataPtr = std::unique_ptr<PolyhedronData>;

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DStaticBox& data) override;
    void addEntity(EntityId id, const DDynamicBox& data) override;
    void addEntity(EntityId id, const DTerrainChunk& data) override;
    void addEntity(EntityId id, const DPolyhedron& data) override;
    void addEntity(EntityId id, const DCapsule& data) override;
    void addEntity(EntityId id, const DAggregate& data) override;

    void setInverseMass(EntityId id, float inverseMass) override;
    void applyForce(EntityId id, const Vec3f& force, float seconds) override;
    void applyTorque(EntityId id, const Vec3f& torque, float seconds) override;
    void setStationary(EntityId id) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    Ecs& m_ecs;
    std::unordered_map<EntityId, std::vector<EntityId>> m_aggregates;
    std::unordered_map<EntityId, PolyhedronDataPtr> m_polyhedra;

    void applyForce(CCollisionDynamic& comp, const Force& force);
    void applyTorque(CCollisionDynamic& dynamic, CCollisionRotational& rotational,
      const Force& torque);
    void applyGravity(CCollisionDynamic& comp);
    std::vector<CollisionPair> findPossibleCollisions();
    std::vector<Contact> generateContacts(const std::vector<CollisionPair>& pairs);
    void integrate();

    void generateBoxPolyContacts(const ObjectComponents& A, const ObjectComponents& B,
      std::vector<Contact>& contacts) const;

    void generateCapsulePolyContacts(const ObjectComponents& A, const ObjectComponents& B,
      std::vector<Contact>& contacts) const;
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DStaticBox& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionBox>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionBox>(id) = CCollisionBox{
    .boundingBox = data.boundingBox
  };
}

void SysCollisionImpl::addEntity(EntityId id, const DDynamicBox& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionBox>(componentStore, id);
  assertHasComponent<CCollisionDynamic>(componentStore, id);
  assertHasComponent<CCollisionRotational>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionBox>(id) = CCollisionBox{
    .boundingBox = data.boundingBox
  };

  CCollisionDynamic dynamic{
    .inverseMass = data.inverseMass,
    .centreOfMass = data.centreOfMass,
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .resolverDeltaLinearV{},
    .resolverNumAdjustments = 0,
    .framesIdle = 0,
    .idle = false,
    .hasCollided = false
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;

  componentStore.instantiate<CCollisionRotational>(id) = CCollisionRotational{
    .inverseInertialTensor = computeInverseInertialTensor(data.boundingBox, data.inverseMass),
    .torques{},
    .angularAcceleration{},
    .angularVelocity{},
    .resolverDeltaAngularV{}
  };
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionTerrain>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionTerrain>(id) = CCollisionTerrain{
    .heightMap = data.heightMap
  };
}

void SysCollisionImpl::addEntity(EntityId id, const DPolyhedron& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = true
  };

  auto poly = std::make_unique<PolyhedronData>();
  poly->centreOfMass = data.centreOfMass;

  // TODO

  m_polyhedra[id] = std::move(poly);
}

void SysCollisionImpl::addEntity(EntityId id, const DCapsule& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionDynamic>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionCapsule>(id) = CCollisionCapsule{
    .capsule = data.capsule
  };

  CCollisionDynamic dynamic{
    .inverseMass = data.inverseMass,
    .centreOfMass = data.centreOfMass,
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .resolverDeltaLinearV{},
    .resolverNumAdjustments = 0,
    .framesIdle = 0,
    .idle = false,
    .hasCollided = false
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;
}

void SysCollisionImpl::addEntity(EntityId id, const DAggregate& data)
{
  ASSERT(data.boxes.size() == data.boxTransforms.size(), "Incorrect number of box transforms");
  ASSERT(data.polyhedra.size() == data.polyhedraTransforms.size(),
    "Incorrect number of polyhedron transforms");

  auto& sysSpatial = m_ecs.system<SysSpatial>();

  for (size_t i = 0; i < data.boxes.size(); ++i) {
    auto& box = data.boxes[i];

    auto childId = m_ecs.idGen().getNewEntityId();
    m_ecs.componentStore().allocate<DSpatial, DStaticBox>(childId);

    DSpatial spatial{
      .transform = data.boxTransforms[i],
      .parent = id,
      .enabled = true,
      .aabb = transformAabb({
        .min = box.boundingBox.min,
        .max = box.boundingBox.max
      }, box.boundingBox.transform)
    };

    sysSpatial.addEntity(childId, spatial);

    addEntity(childId, box);

    m_aggregates[id].push_back(childId);
  }

  for (size_t i = 0; i < data.polyhedra.size(); ++i) {
    auto childId = m_ecs.idGen().getNewEntityId();
    m_ecs.componentStore().allocate<DSpatial, DPolyhedron>(childId);

    DSpatial spatial{
      .transform = data.polyhedraTransforms[i],
      .parent = id,
      .enabled = true
    };

    sysSpatial.addEntity(childId, spatial);

    addEntity(childId, data.polyhedra[i]);

    m_aggregates[id].push_back(childId);
  }
}

void SysCollisionImpl::removeEntity(EntityId id)
{
  m_polyhedra.erase(id);

  auto i = m_aggregates.find(id);
  if (i != m_aggregates.end()) {
    for (auto childId : i->second) {
      m_ecs.removeEntity(childId);
    }
    m_aggregates.erase(i);
  }
}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{
  return m_ecs.componentStore().hasComponentForEntity<CCollision>(entityId);
}

void SysCollisionImpl::applyGravity(CCollisionDynamic& comp)
{
  if (comp.inverseMass == 0.f) {
    return;
  }

  float f = G / comp.inverseMass;

  applyForce(comp, {
    .force{ 0.f, f, 0.f },
    .lifetime = std::numeric_limits<uint32_t>::max()
  });
}

void SysCollisionImpl::applyForce(EntityId id, const Vec3f& force, float seconds)
{
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  applyForce(comp, {
    .force = force,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyForce(CCollisionDynamic& comp, const Force& force)
{
  uint32_t i = findFreeIndex(comp.linearForces);
  if (i == MAX_FORCES) {
    m_logger.warn("Max forces exceeded on object");
    return;
  }
  comp.linearForces[i] = force;
  comp.idle = false;
  comp.framesIdle = 0;
}

void SysCollisionImpl::applyTorque(EntityId id, const Vec3f& torque, float seconds)
{
  auto& dynamic = m_ecs.componentStore().component<CCollisionDynamic>(id);
  auto& rotational = m_ecs.componentStore().component<CCollisionRotational>(id);
  applyTorque(dynamic, rotational, {
    .force = torque,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyTorque(CCollisionDynamic& dynamic, CCollisionRotational& rotational,
  const Force& torque)
{
  uint32_t i = findFreeIndex(rotational.torques);
  if (i == MAX_FORCES) {
    m_logger.warn("Max torques exceeded on object");
    return;
  }
  rotational.torques[i] = torque;
  dynamic.idle = false;
  dynamic.framesIdle = 0;
}

void SysCollisionImpl::setStationary(EntityId id)
{
  auto& dynamic = m_ecs.componentStore().component<CCollisionDynamic>(id);
  dynamic.linearAcceleration = {};
  dynamic.linearVelocity = {};
  dynamic.linearForces = {};

  if (m_ecs.componentStore().hasComponentForEntity<CCollisionRotational>(id)) {
    auto& rotational = m_ecs.componentStore().component<CCollisionRotational>(id);
    rotational.angularAcceleration = {};
    rotational.angularVelocity = {};
    rotational.torques = {};
  }

  if (dynamic.inverseMass != 0.f) {
    applyGravity(dynamic);
  }
}

void SysCollisionImpl::setInverseMass(EntityId id, float inverseMass)
{
  auto& dynamic = m_ecs.componentStore().component<CCollisionDynamic>(id);

  bool shouldApplyGravity = dynamic.inverseMass == 0.f && inverseMass != 0.f;

  dynamic.inverseMass = inverseMass;

  if (m_ecs.componentStore().hasComponentForEntity<CCollisionRotational>(id)) {
    auto& rotational = m_ecs.componentStore().component<CCollisionRotational>(id);
    auto& box = m_ecs.componentStore().component<CCollisionBox>(id);

    rotational.inverseInertialTensor = computeInverseInertialTensor(box.boundingBox,
      dynamic.inverseMass);
  }

  if (shouldApplyGravity) {
    applyGravity(dynamic);
  }
}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  // Extremely naive collision algorithm
  // TODO: Use something like sweep & prune instead: https://leanrada.com/notes/sweep-and-prune/

  std::vector<CollisionPair> pairs;

  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox, CCollision
  >();

  size_t outerIdx = 0;
  for (auto& group1 : groups) {
    auto flagsComps1 = group1.components<CSpatialFlags>();
    auto localTs1 = group1.components<CLocalTransform>();
    auto globalTs1 = group1.components<CGlobalTransform>();
    auto boundingBoxes1 = group1.components<CBoundingBox>();
    auto collComps1 = group1.components<CCollision>();
    auto collBoxComps1 = group1.components<CCollisionBox>();
    auto collDynamicComps1 = group1.components<CCollisionDynamic>();
    auto collRotationalComps1 = group1.components<CCollisionRotational>();
    auto collTerrainComps1 = group1.components<CCollisionTerrain>();
    auto collCapsuleComps1 = group1.components<CCollisionCapsule>();
    auto entityIds1 = group1.entityIds();

    for (size_t i = 0; i < entityIds1.size(); ++i) {
      auto& flags = flagsComps1[i].flags;
      bool aIsInactive = collDynamicComps1.empty() || collDynamicComps1[i].idle;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {

        size_t innerIdx = 0;
        for (auto& group2 : groups) {
          auto flagsComps2 = group2.components<CSpatialFlags>();
          auto localTs2 = group2.components<CLocalTransform>();
          auto globalTs2 = group2.components<CGlobalTransform>();
          auto boundingBoxes2 = group2.components<CBoundingBox>();
          auto collComps2 = group2.components<CCollision>();
          auto collBoxComps2 = group2.components<CCollisionBox>();
          auto collDynamicComps2 = group2.components<CCollisionDynamic>();
          auto collRotationalComps2 = group2.components<CCollisionRotational>();
          auto collTerrainComps2 = group2.components<CCollisionTerrain>();
          auto collCapsuleComps2 = group2.components<CCollisionCapsule>();
          auto entityIds2 = group2.entityIds();

          for (size_t j = 0; j < entityIds2.size(); ++j) {
            if (innerIdx <= outerIdx) {
              ++innerIdx;
              continue;
            }

            auto& flagsJ = flagsComps2[j].flags;
            bool bIsInactive = collDynamicComps2.empty() || collDynamicComps2[j].idle;

            if (flagsJ.test(SpatialFlags::ParentEnabled) && flagsJ.test(SpatialFlags::Enabled)) {
              if (aIsInactive && bIsInactive) {
                ++innerIdx;
                continue;
              }

              auto& box1 = boundingBoxes1[i].worldSpaceAabb;
              auto& box2 = boundingBoxes2[j].worldSpaceAabb;

              if (box1.max[0] >= box2.min[0] && box1.min[0] <= box2.max[0] &&
                box1.max[1] >= box2.min[1] && box1.min[1] <= box2.max[1] &&
                box1.max[2] >= box2.min[2] && box1.min[2] <= box2.max[2]) {

                pairs.push_back({
                  .A = {
                    .entityId = entityIds1[i],
                    .localTransform = &localTs1[i],
                    .globalTransform = collDynamicComps1.empty() ? &globalTs1[i] : nullptr,
                    .spatialFlags = &flagsComps1[i],
                    .collision = &collComps1[i],
                    .dynamic = collDynamicComps1.empty() ? nullptr : &collDynamicComps1[i],
                    .rotational = collRotationalComps1.empty() ? nullptr : &collRotationalComps1[i],
                    .box = collBoxComps1.empty() ? nullptr : &collBoxComps1[i],
                    .capsule = collCapsuleComps1.empty() ? nullptr : &collCapsuleComps1[i],
                    .terrain = collTerrainComps1.empty() ? nullptr : &collTerrainComps1[i]
                  },
                  .B = {
                    .entityId = entityIds2[j],
                    .localTransform = &localTs2[j],
                    .globalTransform = collDynamicComps2.empty() ? &globalTs2[j] : nullptr,
                    .spatialFlags = &flagsComps2[j],
                    .collision = &collComps2[j],
                    .dynamic = collDynamicComps2.empty() ? nullptr : &collDynamicComps2[j],
                    .rotational = collRotationalComps2.empty() ? nullptr : &collRotationalComps2[j],
                    .box = collBoxComps2.empty() ? nullptr : &collBoxComps2[j],
                    .capsule = collCapsuleComps2.empty() ? nullptr : &collCapsuleComps2[j],
                    .terrain = collTerrainComps2.empty() ? nullptr : &collTerrainComps2[j]
                  },
                });
              }
            }

            ++innerIdx;
          }
        }
      }

      ++outerIdx;
    }
  }

  return pairs;
}

inline bool isInside(const BoundingBox& box, const Vec3f& P, float epsilon = 0.f)
{
  return
    P[0] > box.min[0] - epsilon && P[0] < box.max[0] + epsilon &&
    P[1] > box.min[1] - epsilon && P[1] < box.max[1] + epsilon &&
    P[2] > box.min[2] - epsilon && P[2] < box.max[2] + epsilon;
}

// Returns box's vertices in world space
inline std::array<Vec3f, 8> getVertices(const BoundingBox& box, const Mat4x4f& transform)
{
  // C------D
  // |\     |\
  // | \    | \
  // A--\---B  \
  //  \  \   \  \
  //   \  G------H
  //    \ |    \ |
  //     \|     \|
  //      E------F
  auto t = transform * box.transform;
  return {
    (t * Vec4f{ box.min[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // A  
    (t * Vec4f{ box.max[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // B
    (t * Vec4f{ box.min[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // C
    (t * Vec4f{ box.max[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // D
    (t * Vec4f{ box.min[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // E
    (t * Vec4f{ box.max[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // F
    (t * Vec4f{ box.min[0], box.max[1], box.max[2], 1.f }).sub<3>(),  // G
    (t * Vec4f{ box.max[0], box.max[1], box.max[2], 1.f }).sub<3>()   // H
  };
}

// Returns box's inverse tangents at each vertex in world space
inline std::array<Vec3f, 8> getInverseTangents(const BoundingBox& box, const Mat4x4f& transform)
{
  const float k = 0.57735026919f; // One over root 3
  auto t = transform * box.transform;

  return {
    (t * Vec4f{ k, k, k, 0.f }).sub<3>(),   // A  
    (t * Vec4f{ -k, k, k, 0.f }).sub<3>(),  // B
    (t * Vec4f{ k, -k, k, 0.f }).sub<3>(),  // C
    (t * Vec4f{ -k, -k, k, 0.f }).sub<3>(), // D
    (t * Vec4f{ k, k, -k, 0.f }).sub<3>(),  // E
    (t * Vec4f{ -k, k, -k, 0.f }).sub<3>(), // F
    (t * Vec4f{ k, -k, -k, 0.f }).sub<3>(), // G
    (t * Vec4f{ -k, -k, -k, 0.f }).sub<3>() // H
  };
}

inline std::array<Edge, 12> getEdges(const std::array<Vec3f, 8>& vertices)
{
  return {
    Edge{ vertices[0], vertices[1] }, // AB
    Edge{ vertices[1], vertices[3] }, // BD
    Edge{ vertices[3], vertices[2] }, // DC
    Edge{ vertices[2], vertices[0] }, // CA
    Edge{ vertices[4], vertices[5] }, // EF
    Edge{ vertices[5], vertices[7] }, // FH
    Edge{ vertices[7], vertices[6] }, // HG
    Edge{ vertices[6], vertices[4] }, // GE
    Edge{ vertices[0], vertices[4] }, // AE
    Edge{ vertices[1], vertices[5] }, // BF
    Edge{ vertices[2], vertices[6] }, // CG
    Edge{ vertices[3], vertices[7] }, // DH
  };
}

// Returns penetration depth or 0 if there's no penetration
//
// P is the point in world space
// invTangent is the inverse tangent at P
float pointBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Vec3f& P, const Vec3f& invTangent, Vec3f& normal)
{
  auto Q = (worldToBoxSpace * Vec4f{ P, { 1.f }}).sub<3>();
  auto V = (worldToBoxSpace * Vec4f{ invTangent, { 0.f }}).sub<3>();

  if (!isInside(box, Q)) {
    return 0.f;
  }

  float w = std::min(std::min(box.max[0] - box.min[0], box.max[1] - box.min[1]),
    box.max[2] - box.min[2]);

  float maxScore = std::numeric_limits<float>::lowest();
  int face = -1; // 0 min-x, 1 max-x, 2 min-y, 3 max-y, 4 min-z, 5 max-z
  float contactPenetration = 0.f;

  // min-x (left) face
  float alignment = V.dot({ -1.f, 0.f, 0.f });
  float penetration = Q[0] - box.min[0];
  float proximity = penetration / w;
  float score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 0;
    contactPenetration = penetration;
  }
  // max-x (right) face
  alignment = V.dot({ 1.f, 0.f, 0.f });
  penetration = box.max[0] - Q[0];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 1;
    contactPenetration = penetration;
  }
  // min-y (bottom) face
  alignment = V.dot({ 0.f, -1.f, 0.f });
  penetration = Q[1] - box.min[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 2;
    contactPenetration = penetration;
  }
  // max-y (top) face
  alignment = V.dot({ 0.f, 1.f, 0.f });
  penetration = box.max[1] - Q[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 3;
    contactPenetration = penetration;
  }
  // min-z (far) face
  alignment = V.dot({ 0.f, 0.f, -1.f });
  penetration = Q[2] - box.min[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 4;
    contactPenetration = penetration;
  }
  // max-z (near) face
  alignment = V.dot({ 0.f, 0.f, 1.f });
  penetration = box.max[2] - Q[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    maxScore = score;
    face = 5;
    contactPenetration = penetration;
  }

  // Set w components to zero to ignore translation
  switch (face) {
    case 0: // min-x (left) face
      normal = (boxToWorldSpace * Vec4f{ -1.f, 0.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 1: // max-x (right) face
      normal = (boxToWorldSpace * Vec4f{ 1.f, 0.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 2: // min-y (bottom) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, -1.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 3: // max-y (top) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 1.f, 0.f, 0.f }).sub<3>().normalise();
      break;
    case 4: // min-z (far) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 0.f, -1.f, 0.f }).sub<3>().normalise();
      break;
    case 5: // max-z (near) face
      normal = (boxToWorldSpace * Vec4f{ 0.f, 0.f, 1.f, 0.f }).sub<3>().normalise();
      break;
    default:
      return 0.f;
  }

  return contactPenetration;
}

inline Vec3f differentVector(const Vec3f& v)
{
  static auto r = rotationMatrix3x3({ 1.f, 2.f, 3.f });
  return r * v;
}

// For static objects, we read their global transform
inline Mat4x4f& getTransform(const ObjectComponents& obj)
{
  return obj.globalTransform ? obj.globalTransform->transform : obj.localTransform->transform;
}

bool boxXBoxPointContact(const ObjectComponents& A, const ObjectComponents& B, Contact& contact)
{
  auto boxBToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxBSpace = inverse(boxBToWorldSpace);

  float maxPenetration = 0.f;
  int vertWithMaxPenetration = -1;
  std::array<Vec3f, 8> normals;

  auto verts = getVertices(A.box->boundingBox, getTransform(A));
  auto invTangents = getInverseTangents(A.box->boundingBox, getTransform(A));

  for (size_t i = 0; i < 8; ++i) {
    float penetration = pointBoxPenetration(B.box->boundingBox, worldToBoxBSpace, boxBToWorldSpace,
      verts[i], invTangents[i], normals[i]);

    if (penetration > maxPenetration) {
      vertWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (vertWithMaxPenetration == -1) {
    return false;
  }

  contact.A = A;
  contact.B = B;
  contact.normal = normals[vertWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = verts[vertWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

bool boxBoxPointContact(const ObjectComponents& A, const ObjectComponents& B, Contact& contact)
{
  if (boxXBoxPointContact(A, B, contact)) {
    return true;
  }
  return boxXBoxPointContact(B, A, contact);
}

// Solve system of equations for s and t:
// l1: As + Bt + C = 0
// l2: Ds + Et + F = 0
// Returns false if no solution (lines are parallel)
//
// Method was derived by simply rearranging l1 to find s in terms of t, then substituting into l2
// and solving for t. Vice-versa yields s. Both results had a common denominator, here referred to
// as the determinant.
inline bool solve(const Vec3f& l1, const Vec3f& l2, Vec2f& st)
{
  float determinant = l1[0] * l2[1] - l1[1] * l2[0];
  if (fabs(determinant) < 0.00000001f) {
    return false;
  }
  st = {
    (l1[1] * l2[2] - l1[2] * l2[1]) / determinant,
    (l1[2] * l2[0] - l2[2] * l1[0]) / determinant
  };
  return true;
}

// Returns penetration depth or 0 if there's no penetration
float edgeBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Edge& worldSpaceEdge, Vec3f& point, Vec3f& normal)
{
  Edge edge{
    .A = (worldToBoxSpace * Vec4f{ worldSpaceEdge.A, { 1.f }}).sub<3>(),
    .B = (worldToBoxSpace * Vec4f{ worldSpaceEdge.B, { 1.f }}).sub<3>()
  };

  if (
    (edge.A[0] < box.min[0] && edge.B[0] < box.min[0]) ||
    (edge.A[1] < box.min[1] && edge.B[1] < box.min[1]) ||
    (edge.A[2] < box.min[2] && edge.B[2] < box.min[2]) ||
    (edge.A[0] > box.max[0] && edge.B[0] > box.max[0]) ||
    (edge.A[1] > box.max[1] && edge.B[1] > box.max[1]) ||
    (edge.A[2] > box.max[2] && edge.B[2] > box.max[2])
  ) {
    return 0.f;
  }

  // Line: a + sv
  Vec3f a = worldSpaceEdge.A;
  Vec3f v = worldSpaceEdge.B - worldSpaceEdge.A;

  auto boxEdges = getEdges(getVertices(box, boxToWorldSpace));

  std::array<Vec3f, 12> normals;
  std::array<Vec3f, 12> points;
  int indexOfMinPenetration = -1;

  float minSqPenetration = std::numeric_limits<float>::max();

  for (size_t i = 0; i < 12; ++i) {
    auto& boxEdge = boxEdges[i];

    // Line: b + tw
    Vec3f b = boxEdge.A;
    Vec3f w = boxEdge.B - boxEdge.A;

    Vec2f st;
    if (!solve({
      -v.squareMagnitude(),
      v.dot(w),
      v.dot(b) - v.dot(a)
    },
    {
      -v.dot(w),
      w.squareMagnitude(),
      w.dot(b) - w.dot(a)
    }, st)) {
      continue;
    }

    if (!inRange(st[0], 0.f, 1.f)) {
      continue;
    }

    if (!inRange(st[1], 0.f, 1.f)) {
      continue;
    }

    Vec3f P = a + v * st[0];
    Vec3f Q = b + w * st[1];

    auto PQ = Q - P;
    float sqPenetration = PQ.squareMagnitude();

    // TODO: Replace with distance to centre test
    if (!isInside(box, (worldToBoxSpace * Vec4f{ P[0], P[1], P[2], 1.f }).sub<3>(), 0.002f)) {
      continue;
    }

    if (sqPenetration < minSqPenetration) {
      minSqPenetration = sqPenetration;
      indexOfMinPenetration = i;
      points[i] = P + PQ * 0.5f;
      normals[i] = PQ.normalise();
    }
  }

  if (indexOfMinPenetration != -1) {
    normal = normals[indexOfMinPenetration];
    point = points[indexOfMinPenetration];

    return sqrtf(minSqPenetration);
  }

  return 0.f;
}

bool boxBoxEdgeContact(const ObjectComponents& A, const ObjectComponents& B, Contact& contact)
{
  float maxPenetration = 0.f;
  int edgeWithMaxPenetration = -1;
  std::array<Vec3f, 12> points;
  std::array<Vec3f, 12> normals;

  auto boxBToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxBSpace = inverse(boxBToWorldSpace);

  auto edges = getEdges(getVertices(A.box->boundingBox, getTransform(A)));
  for (size_t i = 0; i < 12; ++i) {
    float penetration = edgeBoxPenetration(B.box->boundingBox, worldToBoxBSpace, boxBToWorldSpace,
      edges[i], points[i], normals[i]);

    if (penetration > maxPenetration) {
      edgeWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (edgeWithMaxPenetration == -1) {
    return false;
  }

  contact.A = A;
  contact.B = B;
  contact.normal = normals[edgeWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = points[edgeWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

void generateBoxBoxContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.box != nullptr);

  Contact contact;

  if (boxBoxPointContact(A, B, contact)) {
    contacts.push_back(contact);
  }
  else if (boxBoxEdgeContact(A, B, contact)) {
    contacts.push_back(contact);
  }
}

void generateCapsuleCapsuleContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.capsule != nullptr);
  assert(B.capsule != nullptr);

  // TODO
}

Vec3f closestPoint(const std::array<Vec3f, 3>& triangle, const Vec3f& P)
{
  auto& A = triangle[0];
  auto& B = triangle[1];
  auto& C = triangle[2];

  auto AB = B - A;
  auto AC = C - A;
  auto AP = P - A;

  float APdotAB = AP.dot(AB);
  float APdotAC = AP.dot(AC);

  if (APdotAB < 0.f && APdotAC < 0.f) {
    return A;
  }

  auto BP = P - B;
  auto BA = -AB;
  auto BC = C - B;

  if (BP.dot(BA) < 0.f && BP.dot(BC) < 0.f) {
    return B;
  }

  auto CP = P - C;
  auto CA = -AC;
  auto CB = B - C;

  if (CP.dot(CA) < 0.f && CP.dot(CB) < 0.f) {
    return C;
  }

  // Point expressed in triangle basis
  // P = A + s * AB + t * AC
  // => AP = s * AB + t * AC

  // 3 equations, 2 unknowns is over-determined system
  // Project into 2D with dot product before solving

  // AP . AB = s * AB . AB + t * AC . AB
  // AP . AC = s * AB . AC + t * AC . AC

  float ABdotAB = AB.dot(AB);
  float ABdotAC = AB.dot(AC);
  float ACdotAC = AC.dot(AC);

  Vec2f st;
  assert(solve({ ABdotAB, ABdotAC, -APdotAB }, { ABdotAC, ACdotAC, -APdotAC }, st));

  // Barycentric coordinates
  float beta = st[0];
  float gamma = st[1];
  float alpha = 1.f - gamma - beta;

  float min = std::min(alpha, std::min(beta, gamma));

  if (min < 0.f) {
    if (min == alpha) {
      // Find closest point on line segment BC
      float s = BP.dot(BC) / BC.squareMagnitude();
      return B + BC * s;
    }

    if (min == beta) {
      // Find closest point on line segment AC
      float s = AP.dot(AC) / AC.squareMagnitude();
      return A + AC * s;
    }

    if (min == gamma) {
      // Find closest point on line segment AB
      float s = AP.dot(AB) / AB.squareMagnitude();
      return A + AB * s;
    }
  }

  return A + AB * st[0] + AC * st[1];
}

bool capsuleTerrainFaceContact(const ObjectComponents& A, const ObjectComponents& B,
  Contact& contact)
{
  assert(A.capsule != nullptr);
  assert(B.terrain != nullptr);

  HeightMapSampler sampler{B.terrain->heightMap, getTranslation(getTransform(B))};

  float radius = A.capsule->capsule.radius;

  // Calculate centre of capsule's bottom sphere
  auto P = getTranslation(getTransform(A)) + A.capsule->capsule.translation;
  P[1] = P[1] - A.capsule->capsule.height * 0.5f + radius;

  std::vector<std::array<Vec3f, 3>> triangles;
  Vec2f p{ P[0], P[2] };
  sampler.triangles(p + Vec2f{ -radius, -radius }, p + Vec2f{ radius, radius }, triangles);

  float maxPenetration = 0.f;
  std::vector<Vec3f> points(triangles.size());
  int triangleWithMaxPenetration = -1;

  for (size_t i = 0; i < triangles.size(); ++i) {
    auto& triangle = triangles[i];
    float minX = std::min(std::min(triangle[0][0], triangle[1][0]), triangle[2][0]);
    float maxX = std::max(std::max(triangle[0][0], triangle[1][0]), triangle[2][0]);
    float maxY = std::max(std::max(triangle[0][1], triangle[1][1]), triangle[2][1]);
    float minZ = std::min(std::min(triangle[0][2], triangle[1][2]), triangle[2][2]);
    float maxZ = std::max(std::max(triangle[0][2], triangle[1][2]), triangle[2][2]);

    if (P[1] - radius > maxY) {
      continue;
    }

    if (P[0] - radius > maxX) {
      continue;
    }

    if (P[0] + radius < minX) {
      continue;
    }

    if (P[2] - radius > maxZ) {
      continue;
    }

    if (P[2] + radius < minZ) {
      continue;
    }

    auto Q = closestPoint(triangle, P);
    auto PQ = Q - P;
    float sqDistance = PQ.squareMagnitude();
    if (sqDistance < radius * radius) {
      float penetration = radius - sqrtf(sqDistance);
      if (penetration > maxPenetration) {
        maxPenetration = penetration;
        points[i] = Q;
        triangleWithMaxPenetration = i;
      }
    }
  }

  if (triangleWithMaxPenetration != -1) {
    contact.A = A;
    contact.B = B;
    contact.point = points[triangleWithMaxPenetration];
    contact.normal = (P - contact.point).normalise();
    contact.penetration = maxPenetration;
    contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
    contact.toContactSpace = contact.fromContactSpace.t();

    return true;
  }

  return false;
}

void generateCapsuleTerrainContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.capsule != nullptr);
  assert(B.terrain != nullptr);

  Contact contact;
  if (capsuleTerrainFaceContact(A, B, contact)) {
    contacts.push_back(contact);
  }
}

void generateBoxCapsuleContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.capsule != nullptr);

  // TODO
}

void SysCollisionImpl::generateBoxPolyContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts) const
{
  assert(A.box != nullptr);
  auto i = m_polyhedra.find(B.entityId);
  assert(i != m_polyhedra.end());
  const PolyhedronData& poly = *i->second;

  // TODO
}

void SysCollisionImpl::generateCapsulePolyContacts(const ObjectComponents& A,
  const ObjectComponents& B, std::vector<Contact>& contacts) const
{
  assert(A.capsule != nullptr);
  auto i = m_polyhedra.find(B.entityId);
  assert(i != m_polyhedra.end());
  const PolyhedronData& poly = *i->second;

  // TODO
}

bool boxXTerrainPointContact(const ObjectComponents& A, const ObjectComponents& B, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  HeightMapSampler sampler{B.terrain->heightMap, getTranslation(getTransform(B))};

  auto verts = getVertices(A.box->boundingBox, getTransform(A));
  std::array<Vec3f, 8> normals;

  int vertWithMaxPenetration = -1;
  float maxPenetration = 0.f;

  for (size_t i = 0; i < verts.size(); ++i) {
    auto& vert = verts[i];
    Vec2f p{ vert[0], vert[2] };

    if (!sampler.inRange(p)) {
      continue;
    }

    auto triangle = sampler.triangle(p);
    float maxY = std::max(std::max(triangle[0][1], triangle[1][1]), triangle[2][1]);

    if (vert[1] > maxY) {
      continue;
    }

    auto AB = triangle[1] - triangle[0];
    auto AC = triangle[2] - triangle[0];

    auto n = AB.cross(AC);
    // d = -(Ax + By + Cz)
    auto d = -(n[0] * triangle[0][0] + n[1] * triangle[0][1] + n[2] * triangle[0][2]);

    float y = -(n[0] * vert[0] + n[2] * vert[2] + d) / n[1];

    if (vert[1] > y) {
      continue;
    }

    n = n.normalise();

    float penetration = Vec3f{ 0.f, y - vert[1], 0.f }.dot(n);
    assert(penetration >= 0.f);

    if (penetration > maxPenetration) {
      normals[i] = n;
      maxPenetration = penetration;
      vertWithMaxPenetration = i;
    }
  }

  if (vertWithMaxPenetration != -1) {
    contact.A = A;
    contact.B = B;
    contact.normal = normals[vertWithMaxPenetration];
    contact.point = verts[vertWithMaxPenetration];
    contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
    contact.toContactSpace = contact.fromContactSpace.t();
    contact.penetration = maxPenetration;

    return true;
  }

  return false;
}

bool terrainXBoxPointContact(const ObjectComponents& A, const ObjectComponents& B,
  const Vec2f& boxMin, const Vec2f& boxMax, Contact& contact)
{
  assert(A.terrain != nullptr);
  assert(B.box != nullptr);

  HeightMapSampler sampler{A.terrain->heightMap, getTranslation(getTransform(A))};

  std::vector<Vec3f> terrainVerts;
  sampler.vertices(boxMin, boxMax, terrainVerts);

  std::vector<Vec3f> normals(terrainVerts.size());   // TODO: Avoid std::vector?
  int vertWithMaxPenetration = -1;
  float maxPenetration = 0.f;

  auto boxToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxSpace = inverse(boxToWorldSpace);

  for (size_t i = 0; i < terrainVerts.size(); ++i) {
    auto& vert = terrainVerts[i];
    Vec3f invTangent{ 0.f, -1.f, 0.f }; // TODO: Good enough?

    float penetration = pointBoxPenetration(B.box->boundingBox, worldToBoxSpace, boxToWorldSpace,
      vert, invTangent, normals[i]);

    if (penetration > maxPenetration) {
      maxPenetration = penetration;
      vertWithMaxPenetration = i;
    }
  }

  if (vertWithMaxPenetration != -1) {
    contact.A = A;
    contact.B = B;
    contact.normal = normals[vertWithMaxPenetration];
    contact.point = terrainVerts[vertWithMaxPenetration];
    contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
    contact.toContactSpace = contact.fromContactSpace.t();
    contact.penetration = maxPenetration;

    return true;
  }

  return false;
}

bool boxTerrainPointContact(const ObjectComponents& A, const Vec2f& boxMin, const Vec2f& boxMax,
  const ObjectComponents& B, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  if (boxXTerrainPointContact(A, B, contact)) {
    return true;
  }

  return terrainXBoxPointContact(B, A, boxMin, boxMax, contact);
}

bool boxTerrainEdgeContact(const ObjectComponents& A, const Vec2f& boxMin, const Vec2f& boxMax,
  const ObjectComponents& B, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  HeightMapSampler sampler{B.terrain->heightMap, getTranslation(getTransform(B))};

  std::vector<Edge> terrainEdges;
  sampler.edges(boxMin, boxMax, terrainEdges);

  float maxPenetration = 0.f;
  int edgeWithMaxPenetration = -1;
  std::vector<Vec3f> points(terrainEdges.size());
  std::vector<Vec3f> normals(terrainEdges.size());

  auto boxToWorldSpace = getTransform(A) * A.box->boundingBox.transform;
  auto worldToBoxSpace = inverse(boxToWorldSpace);

  for (size_t i = 0; i < terrainEdges.size(); ++i) {
    float penetration = edgeBoxPenetration(A.box->boundingBox, worldToBoxSpace, boxToWorldSpace,
      terrainEdges[i], points[i], normals[i]);

    if (penetration > maxPenetration) {
      edgeWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (edgeWithMaxPenetration == -1) {
    return false;
  }

  contact.A = A;
  contact.B = B;
  contact.normal = -normals[edgeWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = points[edgeWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

void generateBoxTerrainContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  Vec2f boxMin{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
  Vec2f boxMax{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

  auto boxVerts = getVertices(A.box->boundingBox, getTransform(A));
  for (auto& vert : boxVerts) {
    if (vert[0] < boxMin[0]) {
      boxMin[0] = vert[0];
    }
    if (vert[0] > boxMax[0]) {
      boxMax[0] = vert[0];
    }
    if (vert[2] < boxMin[1]) {
      boxMin[1] = vert[2];
    }
    if (vert[2] > boxMax[1]) {
      boxMax[1] = vert[2];
    }
  }

  Contact contact;

  if (boxTerrainPointContact(A, boxMin, boxMax, B, contact)) {
    contacts.push_back(contact);
  }
  else if (boxTerrainEdgeContact(A, boxMin, boxMax, B, contact)) {
    contacts.push_back(contact);
  }
}

std::vector<Contact> SysCollisionImpl::generateContacts(const std::vector<CollisionPair>& pairs)
{
  std::vector<Contact> contacts;

  for (auto& pair : pairs) {
    if (pair.A.box && pair.B.box) {
      generateBoxBoxContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.box && pair.B.capsule) {
      generateBoxCapsuleContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.box && pair.B.collision->isPolyhedron) {
      generateBoxPolyContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.box && pair.B.terrain) {
      generateBoxTerrainContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.capsule && pair.B.box) {
      generateBoxCapsuleContacts(pair.B, pair.A, contacts);
    }
    else if (pair.A.capsule && pair.B.capsule) {
      generateCapsuleCapsuleContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.capsule && pair.B.collision->isPolyhedron) {
      generateCapsulePolyContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.capsule && pair.B.terrain) {
      generateCapsuleTerrainContacts(pair.A, pair.B, contacts);
    }
    else if (pair.A.collision->isPolyhedron && pair.B.box) {
      generateBoxPolyContacts(pair.B, pair.A, contacts);
    }
    else if (pair.A.collision->isPolyhedron && pair.B.capsule) {
      generateCapsulePolyContacts(pair.B, pair.A, contacts);
    }
    else if (pair.A.terrain && pair.B.box) {
      generateBoxTerrainContacts(pair.B, pair.A, contacts);
    }
    else if (pair.A.terrain && pair.B.capsule) {
      generateCapsuleTerrainContacts(pair.B, pair.A, contacts);
    }
  }

  return contacts;
}

void updateTransform(Mat4x4f& T, const Vec3f& linearV, const Vec3f& angularV)
{
  auto translation = translationMatrix4x4(linearV);
  auto angle = angularV.magnitude();
  auto rotation = rotationMatrix4x4(angularV.normalise(), angle);

  auto currentOffset = getTranslation(T);

  T = translationMatrix4x4(currentOffset) * translation * rotation *
    translationMatrix4x4(-currentOffset) * T;
}

// Constructs a matrix that calculates the velocity induced by a given impulse.
// Its inverse is a matrix that calculates the required impulse for a given velocity.
//
// I is the inverse inertia tensor in world space
// relativePoint is the contact point relative to the centre of mass
inline Mat3x3f computeVelocityMatrix(float inverseMass, const Mat3x3f& I,
  const Vec3f& relativePoint)
{
  Mat3x3f Q = skewSymmetricMatrix(relativePoint);
  Mat3x3f M = scaleMatrix<3>(inverseMass, false);
  return (Q * -1.f) * I * Q + M;
}

void applyPositionDelta(const ObjectComponents& obj, const Vec3f& point, const Vec3f& delta)
{
  if (obj.rotational) {
    Mat3x3f I = transformTensor(obj.rotational->inverseInertialTensor, getTransform(obj));

    auto origin = getTranslation(getTransform(obj));
    auto pointRel = point - (origin + obj.dynamic->centreOfMass);
    Mat3x3f velocityMatrix = computeVelocityMatrix(obj.dynamic->inverseMass, I, pointRel);
    Mat3x3f impulseMatrix = inverse(velocityMatrix);
    Vec3f impulse = impulseMatrix * delta;
    Vec3f linearV = impulse * obj.dynamic->inverseMass;
    Vec3f angularV = I * pointRel.cross(impulse);

    updateTransform(getTransform(obj), linearV, angularV);
  }
  else {
    if (obj.capsule) {
      // TODO: Do something different for capsules?
      updateTransform(getTransform(obj), delta, {});
    }
    else {
      updateTransform(getTransform(obj), delta, {});
    }
  }

  obj.spatialFlags->flags.set(SpatialFlags::Dirty);
}

void resolveInterpenetration(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  float totalInvMass = 0.f;
  if (A.dynamic) {
    totalInvMass += A.dynamic->inverseMass;
  }
  if (B.dynamic) {
    totalInvMass += B.dynamic->inverseMass;
  }
  DBG_ASSERT(totalInvMass != 0.f, "Cannot collide two objects of infinite mass");

  const float margin = 0.f;

  if (A.dynamic && A.dynamic->inverseMass != 0.f) {
    float a = (A.dynamic->inverseMass / totalInvMass) * (contact.penetration + margin);
    auto outDir = contact.normal;

    applyPositionDelta(A, contact.point, outDir * a);
  }

  if (B.dynamic && B.dynamic->inverseMass != 0.f) {
    float b = (B.dynamic->inverseMass / totalInvMass) * (contact.penetration + margin);
    auto outDir = -contact.normal;

    applyPositionDelta(B, contact.point, outDir * b);
  }
}

void reapplyLateralImpulse(Vec3f& impulse, float friction)
{
  float planarImpulse = sqrtf(impulse[0] * impulse[0] + impulse[2] * impulse[2]);
  if (planarImpulse > friction * impulse[1]) {
    impulse[0] /= planarImpulse;
    impulse[2] /= planarImpulse;
    impulse[0] *= friction * impulse[1];
    impulse[2] *= friction * impulse[1];
  }
}

void resolveVelocities(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  bool bothDynamic = A.dynamic && B.dynamic;

  // Interferes with the reapplication of lateral impulse below (for some reason)
  if (!bothDynamic) {
    auto& dynaObj = contact.A.dynamic == nullptr ? contact.B : contact.A;

    if (!dynaObj.dynamic->hasCollided) {
      dynaObj.dynamic->linearVelocity -= { 0.f, G, 0.f };
      dynaObj.dynamic->hasCollided = true;
    }
  }

  if (bothDynamic && (A.dynamic->framesIdle == 0 || B.dynamic->framesIdle == 0)) {
    A.dynamic->framesIdle = 0;
    A.dynamic->idle = false;
    B.dynamic->framesIdle = 0;
    B.dynamic->idle = false;
  }

  Vec3f aPointRel;
  Vec3f bPointRel;

  Vec3f aContactSpaceV;
  if (A.dynamic) {
    auto aOrigin = getTranslation(getTransform(A));
    aPointRel = contact.point - (aOrigin + A.dynamic->centreOfMass);
    auto aTotalWorldSpaceV = A.dynamic->linearVelocity;
    if (A.rotational) {
      aTotalWorldSpaceV += A.rotational->angularVelocity.cross(aPointRel);
    }
    aContactSpaceV = contact.toContactSpace * aTotalWorldSpaceV;
  }

  Vec3f bContactSpaceV;
  if (B.dynamic) {
    auto bOrigin = getTranslation(getTransform(B));
    bPointRel = contact.point - (bOrigin + B.dynamic->centreOfMass);
    auto bTotalWorldSpaceV = B.dynamic->linearVelocity;
    if (B.rotational) {
      bTotalWorldSpaceV += B.rotational->angularVelocity.cross(bPointRel);
    }
    bContactSpaceV = contact.toContactSpace * bTotalWorldSpaceV;
  }

  auto contactSpaceClosingV = bContactSpaceV - aContactSpaceV;

  float r = A.collision->restitution + B.collision->restitution;
  Vec3f desiredContactSpaceClosingV = { 0.f, contactSpaceClosingV[1] * -r, 0.f };
  auto desiredContactSpaceDv = contactSpaceClosingV - desiredContactSpaceClosingV;

  Mat3x3f velocityMatrix;
  Mat3x3f aI;
  Mat3x3f bI;

  if (A.dynamic) {
    if (A.rotational) {
      aI = transformTensor(A.rotational->inverseInertialTensor, getTransform(A));
      velocityMatrix += computeVelocityMatrix(A.dynamic->inverseMass, aI, aPointRel);
    }
    else {
      velocityMatrix += scaleMatrix<3>(A.dynamic->inverseMass, false);
    }
  }

  if (B.dynamic) {
    if (B.rotational) {
      bI = transformTensor(B.rotational->inverseInertialTensor, getTransform(B));
      velocityMatrix += computeVelocityMatrix(B.dynamic->inverseMass, bI, bPointRel);
    }
    else {
      velocityMatrix += scaleMatrix<3>(B.dynamic->inverseMass, false);
    }
  }

  Mat3x3f contactSpaceVelocityMatrix = contact.toContactSpace * velocityMatrix
    * contact.fromContactSpace;
  Mat3x3f impulseMatrix = inverse(contactSpaceVelocityMatrix);

  Vec3f contactSpaceImpulse = impulseMatrix * desiredContactSpaceDv;

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  if (bothDynamic) {
    float friction = 0.5f * (A.collision->friction + B.collision->friction);
    reapplyLateralImpulse(impulse, friction);
  }

  if (A.dynamic) {
    A.dynamic->resolverDeltaLinearV += impulse * A.dynamic->inverseMass;
    if (A.rotational) {
      A.rotational->resolverDeltaAngularV += aI * aPointRel.cross(impulse);
    }
    ++A.dynamic->resolverNumAdjustments;
  }

  if (B.dynamic) {
    B.dynamic->resolverDeltaLinearV += -impulse * B.dynamic->inverseMass;
    if (B.rotational) {
      B.rotational->resolverDeltaAngularV += bI * bPointRel.cross(-impulse);
    }
    ++B.dynamic->resolverNumAdjustments;
  }
}

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CCollision, CCollisionDynamic
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto dynamicComps = group.components<CCollisionDynamic>();
    auto rotationalComps = group.components<CCollisionRotational>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& dynamic = dynamicComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)
        && !dynamic.idle && dynamic.inverseMass != 0.f) {

        if (dynamic.resolverNumAdjustments > 0) {
          dynamic.linearVelocity += dynamic.resolverDeltaLinearV / dynamic.resolverNumAdjustments;
          if (!rotationalComps.empty()) {
            rotationalComps[i].angularVelocity +=
              rotationalComps[i].resolverDeltaAngularV / dynamic.resolverNumAdjustments;
            rotationalComps[i].resolverDeltaAngularV = {};
          }
          dynamic.resolverDeltaLinearV = {};
          dynamic.resolverNumAdjustments = 0;
        }

        // Tweak these idle thresholds
        const float idleMaxLinearV = 0.02f;
        const float idleMaxAngularV = 0.0005f;
        const size_t idleSeconds = 3;

        if (!rotationalComps.empty() && dynamic.linearVelocity.squareMagnitude() < idleMaxLinearV &&
          rotationalComps[i].angularVelocity.squareMagnitude() < idleMaxAngularV) {

          if (++dynamic.framesIdle > TICKS_PER_SECOND * idleSeconds) {
            DBG_LOG(m_logger, "Setting idle");
            dynamic.idle = true;
            dynamic.framesIdle = 0;
            dynamic.linearVelocity = {};
            dynamic.linearAcceleration = {};
            rotationalComps[i].angularVelocity = {};
            rotationalComps[i].angularAcceleration = {};
            continue;
          }
        }
        else {
          //DBG_LOG(m_logger, STR("dv " << dynamic.linearVelocity.squareMagnitude()));
          //DBG_LOG(m_logger, STR("da " << dynamic.angularVelocity.squareMagnitude()));
        }

        Vec3f totalForce;
        Vec3f totalTorque;

        for (size_t f = 0; f < MAX_FORCES; ++f) {
          auto& force = dynamic.linearForces[f];

          if (force.lifetime > 0) {
            totalForce += force.force;
            --force.lifetime;
          }

          if (!rotationalComps.empty()) {
            auto& torque = rotationalComps[i].torques[f];
            if (torque.lifetime > 0) {
              totalTorque += torque.force;
              --torque.lifetime;
            }
          }
        }

        if (!rotationalComps.empty()) {
          updateTransform(localT[i].transform, dynamic.linearVelocity,
            rotationalComps[i].angularVelocity);
        }
        else {
          localT[i].transform = translationMatrix4x4(dynamic.linearVelocity) * localT[i].transform;
        }

        flagsComps[i].flags.set(SpatialFlags::Dirty);

        dynamic.linearVelocity += dynamic.linearAcceleration;
        dynamic.linearAcceleration = totalForce * dynamic.inverseMass;

        if (!rotationalComps.empty()) {
          rotationalComps[i].angularVelocity += rotationalComps[i].angularAcceleration;
          rotationalComps[i].angularAcceleration =
            rotationalComps[i].inverseInertialTensor * totalTorque;
        }

        dynamic.hasCollided = false;
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  size_t maxIterations = 4;
  auto pairs = findPossibleCollisions();

  size_t i = 0;
  for (; i < maxIterations; ++i) {
    //auto pairs = findPossibleCollisions();  // TODO: Slow!

    auto contacts = generateContacts(pairs);
    if (contacts.empty()) {
      break;
    }

    for (auto& contact : contacts) {
      resolveInterpenetration(contact);
      resolveVelocities(contact);
    }
  }

  if (i == maxIterations) {
    //DBG_LOG(m_logger, "Max iterations reached");
  }

  integrate();
}

void SysCollisionImpl::processEvent(const Event& event)
{
  // TODO
}

} // namespace

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysCollisionImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d

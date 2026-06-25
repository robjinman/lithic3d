#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/input.hpp"
#include "lithic3d/xml.hpp"
#include "lithic3d/xml_utils.hpp"
#include <iostream>

namespace lithic3d
{

Triangle HeightMapSampler::triangle(const Vec2f& p) const
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
    Triangle{ A, C, D } :
    Triangle{ A, B, C };
}

void HeightMapSampler::triangles(const Vec2f& min, const Vec2f& max,
  std::vector<Triangle>& triangles) const
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

struct Sphere
{
  Vec3f centre;
  float radius;
};

inline Vec3f calcCentre(const Vec3f& min, const Vec3f& max, const Mat4x4f& transform)
{
  return (transform * Vec4f{ min + (max - min) * 0.5f, { 1.f }}).sub<3>();
}

BoundingBox constructBoundingBox(const XmlNode& xmlBoundingBox)
{
  return {
    .min = metresToWorldUnits(constructVec3f(*xmlBoundingBox.child("min"))),
    .max = metresToWorldUnits(constructVec3f(*xmlBoundingBox.child("max"))),
    .transform = constructTransform(*xmlBoundingBox.child("transform"))
  };
}

Ovoid constructOvoid(const XmlNode& xmlOvoid)
{
  return {
    .radius = metresToWorldUnits(std::stof(xmlOvoid.attribute("radius"))),
    .transform = constructTransform(*xmlOvoid.child("transform"))
  };
}

Cylinder constructCylinder(const XmlNode& xmlCylinder)
{
  return {
    .radius = metresToWorldUnits(std::stof(xmlCylinder.attribute("radius"))),
    .height = metresToWorldUnits(std::stof(xmlCylinder.attribute("height"))),
    .transform = constructTransform(*xmlCylinder.child("transform"))
  };
}

// Calculates how much matrix m scales in the direction of v
inline float calcScaleFactor(const Mat4x4f& m, const Vec3f& v)
{
  return (m * Vec4f{ v, { 0.f }}).sub<3>().magnitude() / v.magnitude();
}

XmlNodePtr toXml(const BoundingBox& bbox)
{
  auto xmlBoundingBox = createXmlNode("bounding_box");

  auto xmlMin = createXmlNode("min");
  xmlMin->setAttribute("x", std::to_string(worldUnitsToMetres(bbox.min[0])));
  xmlMin->setAttribute("y", std::to_string(worldUnitsToMetres(bbox.min[1])));
  xmlMin->setAttribute("z", std::to_string(worldUnitsToMetres(bbox.min[2])));

  auto xmlMax = createXmlNode("max");
  xmlMax->setAttribute("x", std::to_string(worldUnitsToMetres(bbox.max[0])));
  xmlMax->setAttribute("y", std::to_string(worldUnitsToMetres(bbox.max[1])));
  xmlMax->setAttribute("z", std::to_string(worldUnitsToMetres(bbox.max[2])));

  auto xmlTransform = toXml(bbox.transform);

  xmlBoundingBox->addChild(std::move(xmlMin));
  xmlBoundingBox->addChild(std::move(xmlMax));
  xmlBoundingBox->addChild(std::move(xmlTransform));

  return xmlBoundingBox;
}

inline Matrix<float, 4, 4> operator*(const Matrix<float, 4, 4>& lhs, const Matrix<float, 4, 4>& rhs)
{
  Matrix<float, 4, 4> m;

  const float* A = lhs.data();
  const float* B = rhs.data();
  float* M = m.data();

  M[0] = A[0] * B[0] + A[4] * B[1] + A[8] * B[2];
  M[4] = A[0] * B[4] + A[4] * B[5] + A[8] * B[6];
  M[8] = A[0] * B[8] + A[4] * B[9] + A[8] * B[10];
  M[12] = A[0] * B[12] + A[4] * B[13] + A[8] * B[14] + A[12];

  M[1] = A[1] * B[0] + A[5] * B[1] + A[9] * B[2];
  M[5] = A[1] * B[4] + A[5] * B[5] + A[9] * B[6];
  M[9] = A[1] * B[8] + A[5] * B[9] + A[9] * B[10];
  M[13] = A[1] * B[12] + A[5] * B[13] + A[9] * B[14] + A[13];

  M[2] = A[2] * B[0] + A[6] * B[1] + A[10] * B[2];
  M[6] = A[2] * B[4] + A[6] * B[5] + A[10] * B[6];
  M[10] = A[2] * B[8] + A[6] * B[9] + A[10] * B[10];
  M[14] = A[2] * B[12] + A[6] * B[13] + A[10] * B[14] + A[14];

  M[15] = 1.f;

  return m;
}

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

inline Mat3x3f computeInverseInertialTensor(const Mat4x4f& transform, const BoundingBox& box,
  float inverseMass)
{
  auto m = transform * box.transform;

  float w = box.max[0] - box.min[0];
  float h = box.max[1] - box.min[1];
  float d = box.max[2] - box.min[2];

  w *= calcScaleFactor(m, { 1.f, 0.f, 0.f });
  h *= calcScaleFactor(m, { 0.f, 1.f, 0.f });
  d *= calcScaleFactor(m, { 0.f, 0.f, 1.f });

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
  CBoundingBox* aabb = nullptr;
  CCollision* collision = nullptr;
  CCollisionDynamic* dynamic = nullptr;
  CCollisionRotational* rotational = nullptr;
  CCollisionBox* box = nullptr;
  CCollisionCapsule* capsule = nullptr;
  CCollisionSphere* sphere = nullptr;
  CCollisionCylinder* cylinder = nullptr;
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

using PolyEdge = std::array<uint16_t, 2>;
using PolyFace = std::array<uint16_t, 3>;

struct PolyhedronData
{
  std::vector<Vec3f> vertices;
  std::vector<PolyEdge> edges;
  std::vector<PolyFace> faces;
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
    void addEntity(EntityId id, const DSphere& data) override;
    void addEntity(EntityId id, const DCylinder& data) override;
    void addEntity(EntityId id, const DAggregate& data) override;

    CollisionComponentType componentType(EntityId entityId) const override;

    const std::vector<EntityId>& getAggregateChildren(EntityId entityId) const override;
    EntityId addPartToAggregate(EntityId entityId, CollisionComponentType type) override;

    void setInverseMass(EntityId id, float inverseMass) override;
    void applyForce(EntityId id, const Vec3f& force, float seconds) override;
    void applyTorque(EntityId id, const Vec3f& torque, float seconds) override;
    void setStationary(EntityId id) override;

    std::vector<Intersection> getIntersecting(const Vec3f& rayStart,
      const Vec3f& rayEnd) const override;

    const std::string& name() const override;
    void extractComponentSpecs(const ComponentData& data,
      std::vector<ComponentSpec>& specs) const override;
    ComponentDataPtr constructComponentData(const XmlNode& data) const override;
    ComponentDataPtr constructComponentDataWithModifications(const ComponentData& base,
      const XmlNode& changes) const override;
    XmlNodePtr componentToXml(EntityId entityId, EntityId prefabId) const override;
    void addEntity(EntityId id, const ComponentData& data) override;
    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override;

  private:
    Logger& m_logger;
    //EventSystem& m_eventSystem;
    Ecs& m_ecs;
    std::unordered_map<EntityId, std::vector<EntityId>> m_aggregates;
    std::unordered_map<EntityId, PolyhedronDataPtr> m_polyhedra;
    bool m_shouldRebuildSortList = true;
    std::vector<ObjectComponents> m_sortList;

    ComponentDataPtr constructDDynamicBox(const XmlNode& data) const;
    ComponentDataPtr constructDStaticBox(const XmlNode& data) const;
    ComponentDataPtr constructDCapsule(const XmlNode& data) const;
    ComponentDataPtr constructDSphere(const XmlNode& data) const;
    ComponentDataPtr constructDCylinder(const XmlNode& data) const;
    ComponentDataPtr constructDPolyhedron(const XmlNode& data) const;
    ComponentDataPtr constructDAggregate(const XmlNode& data) const;

    XmlNodePtr staticBoxToXml(EntityId entityId) const;
    XmlNodePtr dynamicBoxToXml(EntityId entityId) const;
    XmlNodePtr capsuleToXml(EntityId entityId) const;
    XmlNodePtr sphereToXml(EntityId entityId) const;
    XmlNodePtr cylinderToXml(EntityId entityId) const;
    XmlNodePtr polyhedronToXml(EntityId entityId) const;
    XmlNodePtr aggregateToXml(EntityId entityId) const;

    void rebuildSortList();
    void updateSortList();
    void applyForce(CCollisionDynamic& comp, const Force& force);
    void applyTorque(CCollisionDynamic& dynamic, CCollisionRotational& rotational,
      const Force& torque);
    void applyGravity(CCollisionDynamic& comp);
    std::vector<CollisionPair> findPossibleCollisions();
    std::vector<Contact> generateContacts(const std::vector<CollisionPair>& pairs);
    void integrate();
    void resolveVelocities(const Contact& contact);

    void generateBoxPolyContacts(const ObjectComponents& A, const ObjectComponents& B,
      std::vector<Contact>& contacts) const;

    void generateCapsulePolyContacts(const ObjectComponents& A, const ObjectComponents& B,
      std::vector<Contact>& contacts) const;
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem&, Logger& logger)
  : m_logger(logger)
  //, m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

const std::vector<EntityId>& SysCollisionImpl::getAggregateChildren(EntityId entityId) const
{
  MAP_GET(i, m_aggregates, entityId);
  return i->second;
}

EntityId SysCollisionImpl::addPartToAggregate(EntityId entityId, CollisionComponentType type)
{
  MAP_GET(i, m_aggregates, entityId);
  auto& children = i->second;

  auto& sysSpatial = m_ecs.system<SysSpatial>();

  auto partId = m_ecs.idGen().getNewEntityId();

  switch (type) {
    case CollisionComponentType::StaticBox: {
      m_ecs.componentStore().allocate<DSpatial, DStaticBox>(partId);

      DStaticBox box{};

      DSpatial spatial{
        .transform = identityMatrix<4>(),
        .parent = entityId,
        .enabled = true,
        .aabb = transformAabb({
          .min = box.boundingBox.min,
          .max = box.boundingBox.max
        }, box.boundingBox.transform)
      };

      sysSpatial.addEntity(partId, spatial);
      addEntity(partId, box);

      break;
    }
    // TODO: Polyhedron
    // ...
    default: EXCEPTION("Cannot add part of that type to aggregate");
  }

  children.push_back(partId);

  return partId;
}

CollisionComponentType SysCollisionImpl::componentType(EntityId entityId) const
{
  ASSERT(hasEntity(entityId), "Entity has no collision component");

  auto& componentStore = m_ecs.componentStore();

  bool hasBox = componentStore.hasComponentForEntity<CCollisionBox>(entityId);
  if (hasBox) {
    bool isDynamic = componentStore.hasComponentForEntity<CCollisionDynamic>(entityId);
    return isDynamic ? CollisionComponentType::DynamicBox : CollisionComponentType::StaticBox;
  }

  bool hasCapsule = componentStore.hasComponentForEntity<CCollisionCapsule>(entityId);
  if (hasCapsule) {
    return CollisionComponentType::Capsule;
  }

  bool isTerrain = componentStore.hasComponentForEntity<CCollisionTerrain>(entityId);
  if (isTerrain) {
    return CollisionComponentType::TerrainChunk;
  }

  if (m_aggregates.contains(entityId)) {
    return CollisionComponentType::Aggregate;
  }

  EXCEPTION("Error determining type of collision component");
}

bool boxRayIntersect(const Mat4x4f& transform, const CCollisionBox& boxComp, const Vec3f& rayStart,
  const Vec3f& rayEnd, float& t)
{
  auto& box = boxComp.boundingBox;

  auto boxToWorldSpace = transform * box.transform;
  auto worldToBoxSpace = inverse(boxToWorldSpace);

  auto A = (worldToBoxSpace * Vec4f{rayStart, { 1.f }}).sub<3>();
  auto B = (worldToBoxSpace * Vec4f{rayEnd, { 1.f }}).sub<3>();

  if (
    (A[0] < box.min[0] && B[0] < box.min[0]) ||
    (A[1] < box.min[1] && B[1] < box.min[1]) ||
    (A[2] < box.min[2] && B[2] < box.min[2]) ||
    (A[0] > box.max[0] && B[0] > box.max[0]) ||
    (A[1] > box.max[1] && B[1] > box.max[1]) ||
    (A[2] > box.max[2] && B[2] > box.max[2])
  ) {
    return false;
  }

  // TODO: Slab method
  t = (getTranslation(boxToWorldSpace) - rayEnd).magnitude();

  return true;
}

bool capsuleRayIntersect(const Mat4x4f&, const CCollisionCapsule&, const Vec3f&,
  const Vec3f&, float&)
{
  // TODO
  return false;
}

std::vector<Intersection> SysCollisionImpl::getIntersecting(const Vec3f& rayStart,
  const Vec3f& rayEnd) const
{
  auto& sysSpatial = m_ecs.system<SysSpatial>();
  auto& componentStore = m_ecs.componentStore();
  auto aabbIntersections = sysSpatial.getIntersecting(rayStart, rayEnd);

  std::vector<Intersection> intersections;

  for (auto id : aabbIntersections) {
    float t = 0.f;

    if (componentStore.hasComponentForEntity<CCollisionBox>(id)) {
      auto& box = componentStore.component<CCollisionBox>(id);
      auto& globalT = componentStore.component<CGlobalTransform>(id);
      if (boxRayIntersect(globalT.transform, box, rayStart, rayEnd, t)) {
        intersections.push_back({ id, t });
      }
    }
    else if (m_ecs.componentStore().hasComponentForEntity<CCollisionCapsule>(id)) {
      auto& capsule = m_ecs.componentStore().component<CCollisionCapsule>(id);
      auto& globalT = componentStore.component<CGlobalTransform>(id);
      if (capsuleRayIntersect(globalT.transform, capsule, rayStart, rayEnd, t)) {
        intersections.push_back({ id, t });
      }
    }
    // ...
  }

  std::sort(intersections.begin(), intersections.end(), [](const auto& A, const auto& B) {
    return A.distance < B.distance;
  });

  return intersections;
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

  m_shouldRebuildSortList = true;
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
    .framesIdle = 0,
    .idle = false
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;

  // TODO: Support changes of scale?
  auto& m = componentStore.component<CLocalTransform>(id).transform;

  componentStore.instantiate<CCollisionRotational>(id) = CCollisionRotational{
    .inverseInertialTensor = computeInverseInertialTensor(m, data.boundingBox, data.inverseMass),
    .torques{},
    .angularAcceleration{},
    .angularVelocity{}
  };

  m_shouldRebuildSortList = true;
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

  m_shouldRebuildSortList = true;
}

std::vector<PolyEdge> computePolyhedronEdges(const DPolyhedron& data)
{
  std::vector<PolyEdge> edges;

  // TODO

  return edges;
}

std::vector<PolyFace> computePolyhedronFaces(const DPolyhedron& data)
{
  std::vector<PolyFace> faces;

  // TODO

  return faces;
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
  poly->vertices = data.vertices;
  poly->edges = computePolyhedronEdges(data);
  poly->faces = computePolyhedronFaces(data);

  // TODO: Create component for polyhedron data?

  m_polyhedra[id] = std::move(poly);

  m_shouldRebuildSortList = true;
}

void SysCollisionImpl::addEntity(EntityId id, const DCapsule& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionDynamic>(componentStore, id);
  assertHasComponent<CCollisionCapsule>(componentStore, id);

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
    .centreOfMass{},
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .framesIdle = 0,
    .idle = false
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;

  m_shouldRebuildSortList = true;
}

void SysCollisionImpl::addEntity(EntityId id, const DSphere& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionSphere>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionSphere>(id) = CCollisionSphere{
    .ovoid = data.ovoid
  };

  m_shouldRebuildSortList = true;
}

void SysCollisionImpl::addEntity(EntityId id, const DCylinder& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CLocalTransform>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionCylinder>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction,
    .isPolyhedron = false
  };

  componentStore.instantiate<CCollisionCylinder>(id) = CCollisionCylinder{
    .cylinder = data.cylinder
  };

  m_shouldRebuildSortList = true;
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
      .enabled = true,
      .aabb{} // TODO
    };

    sysSpatial.addEntity(childId, spatial);

    addEntity(childId, data.polyhedra[i]);

    m_aggregates[id].push_back(childId);
  }
}

const std::string& SysCollisionImpl::name() const
{
  static const std::string name = "collision";
  return name;
}

void SysCollisionImpl::extractComponentSpecs(const ComponentData& data,
  std::vector<ComponentSpec>& specs) const
{
  extractSpecs<DDynamicBox>(data, specs);
  extractSpecs<DStaticBox>(data, specs);
  extractSpecs<DCapsule>(data, specs);
  extractSpecs<DSphere>(data, specs);
  extractSpecs<DCylinder>(data, specs);
  extractSpecs<DPolyhedron>(data, specs);
  extractSpecs<DAggregate>(data, specs);
  extractSpecs<DTerrainChunk>(data, specs);
  // ...
}

ComponentDataPtr SysCollisionImpl::constructDDynamicBox(const XmlNode& xmlDynamicBox) const
{
  float invMass = std::stof(xmlDynamicBox.attribute("inverse_mass"));
  float restitution = std::stof(xmlDynamicBox.attribute("restitution"));
  float friction = std::stof(xmlDynamicBox.attribute("friction"));

  return std::make_unique<ComponentDataWrapper<DDynamicBox>>(DDynamicBox{
    .inverseMass = invMass,
    .restitution = restitution,
    .friction = friction,
    .centreOfMass = metresToWorldUnits(constructVec3f(*xmlDynamicBox.child("centre_of_mass"))),
    .boundingBox = constructBoundingBox(*xmlDynamicBox.child("bounding_box"))
  });
}

ComponentDataPtr SysCollisionImpl::constructDStaticBox(const XmlNode& xmlStaticBox) const
{
  float restitution = std::stof(xmlStaticBox.attribute("restitution"));
  float friction = std::stof(xmlStaticBox.attribute("friction"));

  return std::make_unique<ComponentDataWrapper<DStaticBox>>(DStaticBox{
    .restitution = restitution,
    .friction = friction,
    .boundingBox = constructBoundingBox(*xmlStaticBox.child("bounding_box"))
  });
}

ComponentDataPtr SysCollisionImpl::constructDCapsule(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysCollisionImpl::constructDSphere(const XmlNode& xmlSphere) const
{
  float restitution = std::stof(xmlSphere.attribute("restitution"));
  float friction = std::stof(xmlSphere.attribute("friction"));

  return std::make_unique<ComponentDataWrapper<DSphere>>(DSphere{
    .restitution = restitution,
    .friction = friction,
    .ovoid = constructOvoid(*xmlSphere.child("ovoid"))
  });
}

ComponentDataPtr SysCollisionImpl::constructDCylinder(const XmlNode& xmlCylinder) const
{
  float restitution = std::stof(xmlCylinder.attribute("restitution"));
  float friction = std::stof(xmlCylinder.attribute("friction"));

  return std::make_unique<ComponentDataWrapper<DCylinder>>(DCylinder{
    .restitution = restitution,
    .friction = friction,
    .cylinder = constructCylinder(*xmlCylinder.child("cylinder"))
  });
}

ComponentDataPtr SysCollisionImpl::constructDPolyhedron(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysCollisionImpl::constructDAggregate(const XmlNode& xmlAggregate) const
{
  DAggregate aggregate;

  for (auto& xmlAggregatePart : xmlAggregate) {
    auto data = constructComponentData(*xmlAggregatePart.child("collision"));
    auto& xmlTransform = *xmlAggregatePart.child("transform");

    if (data->typeId() == typeid(DStaticBox).hash_code()) {
      auto& wrapper = dynamic_cast<const ComponentDataWrapper<DStaticBox>&>(*data);
      aggregate.boxes.push_back(wrapper.data());
      aggregate.boxTransforms.push_back(constructTransform(xmlTransform));

      assert(aggregate.boxes.size() == aggregate.boxTransforms.size());
    }
    // ...
    else {
      EXCEPTION("Component cannot be part of aggregate");
    }
  }

  return std::make_unique<ComponentDataWrapper<DAggregate>>(std::move(aggregate));
}

ComponentDataPtr SysCollisionImpl::constructComponentData(const XmlNode& xmlSysCollision) const
{
  auto& xmlComp = *xmlSysCollision.begin();
  if (xmlComp.name() == "dynamic_box") {
    return constructDDynamicBox(xmlComp);
  }
  else if (xmlComp.name() == "static_box") {
    return constructDStaticBox(xmlComp);
  }
  else if (xmlComp.name() == "capsule") {
    return constructDCapsule(xmlComp);
  }
  else if (xmlComp.name() == "sphere") {
    return constructDSphere(xmlComp);
  }
  else if (xmlComp.name() == "cylinder") {
    return constructDCylinder(xmlComp);
  }
  else if (xmlComp.name() == "polyhedron") {
    return constructDPolyhedron(xmlComp);
  }
  else if (xmlComp.name() == "aggregate") {
    return constructDAggregate(xmlComp);
  }
  else {
    EXCEPTION("Bad component data type for collision system");
  }
}

ComponentDataPtr SysCollisionImpl::constructComponentDataWithModifications(
  const ComponentData& base, const XmlNode&) const
{
  // TODO

  // For now, just clone the base data and ignore modifications
  if (base.typeId() == typeid(DStaticBox).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DStaticBox>&>(base).data();
    return std::make_unique<ComponentDataWrapper<DStaticBox>>(collision);
  }
  else if (base.typeId() == typeid(DDynamicBox).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DDynamicBox>&>(base).data();
    return std::make_unique<ComponentDataWrapper<DDynamicBox>>(collision);
  }
  else if (base.typeId() == typeid(DAggregate).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DAggregate>&>(base).data();
    return std::make_unique<ComponentDataWrapper<DAggregate>>(collision);
  }
  // TODO
  // ...
  else {
    EXCEPTION("Not implemented")
  }
}

XmlNodePtr SysCollisionImpl::staticBoxToXml(EntityId entityId) const
{
  // TODO: Compare with prefab

  auto& collisionComp = m_ecs.componentStore().component<CCollision>(entityId);
  auto& boxComp = m_ecs.componentStore().component<CCollisionBox>(entityId);

  auto xmlSysCollision = createXmlNode("collision");

  auto xmlBox = createXmlNode("static_box");
  xmlBox->setAttribute("restitution", std::to_string(collisionComp.restitution));
  xmlBox->setAttribute("friction", std::to_string(collisionComp.friction));

  xmlBox->addChild(toXml(boxComp.boundingBox));
  xmlSysCollision->addChild(std::move(xmlBox));

  return xmlSysCollision;
}

XmlNodePtr SysCollisionImpl::dynamicBoxToXml(EntityId entityId) const
{
  // TODO: Compare with prefab

  auto& collisionComp = m_ecs.componentStore().component<CCollision>(entityId);
  auto& boxComp = m_ecs.componentStore().component<CCollisionBox>(entityId);

  auto xmlSysCollision = createXmlNode("collision");

  auto xmlBox = createXmlNode("static_box");
  xmlBox->setAttribute("restitution", std::to_string(collisionComp.restitution));
  xmlBox->setAttribute("friction", std::to_string(collisionComp.friction));

  auto& dynaComp = m_ecs.componentStore().component<CCollisionDynamic>(entityId);

  xmlBox->setAttribute("inverse_mass", std::to_string(dynaComp.inverseMass));

  auto xmlCentre = createXmlNode("centre_of_mass");
  xmlCentre->setAttribute("x", std::to_string(worldUnitsToMetres(dynaComp.centreOfMass[0])));
  xmlCentre->setAttribute("y", std::to_string(worldUnitsToMetres(dynaComp.centreOfMass[1])));
  xmlCentre->setAttribute("z", std::to_string(worldUnitsToMetres(dynaComp.centreOfMass[2])));

  xmlBox->addChild(std::move(xmlCentre));

  xmlBox->addChild(toXml(boxComp.boundingBox));
  xmlSysCollision->addChild(std::move(xmlBox));

  return xmlSysCollision;
}

XmlNodePtr SysCollisionImpl::aggregateToXml(EntityId entityId) const
{
  // TODO: Compare with prefab

  auto& componentStore = m_ecs.componentStore();

  auto xmlSysCollision = createXmlNode("collision");
  auto xmlAggregate = createXmlNode("aggregate");

  auto childrenIds = getAggregateChildren(entityId);

  for (auto childId : childrenIds) {
    auto xmlAggregatePart = createXmlNode("aggregate_part");
    XmlNodePtr xmlChild;

    auto partType = componentType(childId);
    switch (partType) {
      case CollisionComponentType::StaticBox: {
        xmlChild = staticBoxToXml(childId);
        break;
      }
      case CollisionComponentType::Polyhedron: {
        xmlChild = polyhedronToXml(childId);
        break;
      }
      default: EXCEPTION("Error converting to XML; Unexpected component type in aggregate");
    }

    auto& localTransform = componentStore.component<CLocalTransform>(childId);
    auto xmlTransform = toXml(localTransform.transform);

    xmlAggregatePart->addChild(std::move(xmlTransform));
    xmlAggregatePart->addChild(std::move(xmlChild));
    xmlAggregate->addChild(std::move(xmlAggregatePart));
  }

  xmlSysCollision->addChild(std::move(xmlAggregate));

  return xmlSysCollision;
}

XmlNodePtr SysCollisionImpl::capsuleToXml(EntityId) const
{
  // TODO
  return createXmlNode("collision");
}

XmlNodePtr SysCollisionImpl::polyhedronToXml(EntityId) const
{
  // TODO
  return createXmlNode("collision");
}

XmlNodePtr SysCollisionImpl::componentToXml(EntityId entityId, EntityId) const
{
  // TODO: Compare with prefab

  if (!hasEntity(entityId)) {
    return nullptr;
  }

  auto type = componentType(entityId);

  switch (type) {
    case CollisionComponentType::StaticBox: return staticBoxToXml(entityId);
    case CollisionComponentType::DynamicBox: return dynamicBoxToXml(entityId);
    case CollisionComponentType::Capsule: return capsuleToXml(entityId);
    case CollisionComponentType::Aggregate: return aggregateToXml(entityId);
    case CollisionComponentType::Polyhedron: return polyhedronToXml(entityId);
    default: EXCEPTION("Cannot convert component to XML");
  }
}

void SysCollisionImpl::addEntity(EntityId id, const ComponentData& data)
{
  if (data.typeId() == typeid(DStaticBox).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DStaticBox>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DDynamicBox).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DDynamicBox>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DAggregate).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DAggregate>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DCapsule).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DCapsule>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DSphere).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DSphere>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DCylinder).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DCylinder>&>(data).data();
    addEntity(id, collision);
  }
  else if (data.typeId() == typeid(DPolyhedron).hash_code()) {
    auto& collision = dynamic_cast<const ComponentDataWrapper<DPolyhedron>&>(data).data();
    addEntity(id, collision);
  }
  // ...
  else {
    EXCEPTION("Not implemented")
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

  m_shouldRebuildSortList = true;
}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{
  return m_aggregates.contains(entityId) ||
     m_ecs.componentStore().hasComponentForEntity<CCollision>(entityId);
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

  if (inverseMass == 0.f) {
    setStationary(id);
  }

  if (m_ecs.componentStore().hasComponentForEntity<CCollisionRotational>(id)) {
    auto& rotational = m_ecs.componentStore().component<CCollisionRotational>(id);
    auto& box = m_ecs.componentStore().component<CCollisionBox>(id);
    auto& m = m_ecs.componentStore().component<CLocalTransform>(id).transform;

    rotational.inverseInertialTensor = computeInverseInertialTensor(m, box.boundingBox,
      dynamic.inverseMass);
  }

  if (shouldApplyGravity) {
    applyGravity(dynamic);
  }
}

std::vector<CollisionPair> SysCollisionImpl::findPossibleCollisions()
{
  std::vector<CollisionPair> pairs;

  float margin = 0.f;//metresToWorldUnits(0.1f);

  for (size_t i = 0; i < m_sortList.size(); ++i) {
    auto& A = m_sortList[i];
    auto& aAabb = A.aabb->worldSpaceAabb;
    bool aIsInactive = A.dynamic == nullptr || A.dynamic->idle;

    for (size_t j = i + 1; j < m_sortList.size(); ++j) {
      auto& B = m_sortList[j];
      auto& bAabb = B.aabb->worldSpaceAabb;

      if (bAabb.min[0] - margin > aAabb.max[0] + margin) {
        break;
      }

      if (aAabb.min[1] - margin > bAabb.max[1] + margin ||
        bAabb.min[1] - margin > aAabb.max[1] + margin ||
        aAabb.min[2] - margin > bAabb.max[2] + margin ||
        bAabb.min[2] - margin > aAabb.max[2] + margin) {

        continue;
      }

      bool bIsInactive = B.dynamic == nullptr || B.dynamic->idle;

      if (aIsInactive && bIsInactive) {
        continue;
      }

      pairs.push_back({
        .A = A,
        .B = B
      });
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
  // |\     |\        .
  // | \    | \       .
  // A--\---B  \      .
  //  \  \   \  \     .
  //   \  G------H    .
  //    \ |    \ |    .
  //     \|     \|    .
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

inline std::vector<Triangle> getTriangles(const std::array<Vec3f, 8>& vertices)
{
  // Triangles in no particular winding order
  return {
    Triangle{ vertices[0], vertices[2], vertices[1] },    // ACB
    Triangle{ vertices[1], vertices[2], vertices[3] },    // BCD
    Triangle{ vertices[0], vertices[4], vertices[6] },    // AEG
    Triangle{ vertices[0], vertices[6], vertices[2] },    // AGC
    Triangle{ vertices[4], vertices[5], vertices[6] },    // EFG
    Triangle{ vertices[5], vertices[7], vertices[6] },    // FHG
    Triangle{ vertices[5], vertices[1], vertices[7] },    // FBH
    Triangle{ vertices[1], vertices[3], vertices[7] },    // BDH
    Triangle{ vertices[6], vertices[7], vertices[3] },    // GHD
    Triangle{ vertices[6], vertices[2], vertices[3] },    // GCD
    Triangle{ vertices[0], vertices[1], vertices[5] },    // ABF
    Triangle{ vertices[0], vertices[5], vertices[4] }     // AFE
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

  // TODO: Can we simplify this?

  // min-x (left) face
  float alignment = V.dot({ -1.f, 0.f, 0.f });
  float penetration = Q[0] - box.min[0];
  float proximity = penetration / w;
  float score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { -1.f, 0.f, 0.f });
    maxScore = score;
    face = 0;
    contactPenetration = worldSpacePen;
  }
  // max-x (right) face
  alignment = V.dot({ 1.f, 0.f, 0.f });
  penetration = box.max[0] - Q[0];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { 1.f, 0.f, 0.f });
    maxScore = score;
    face = 1;
    contactPenetration = worldSpacePen;
  }
  // min-y (bottom) face
  alignment = V.dot({ 0.f, -1.f, 0.f });
  penetration = Q[1] - box.min[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { 0.f, -1.f, 0.f });
    maxScore = score;
    face = 2;
    contactPenetration = worldSpacePen;
  }
  // max-y (top) face
  alignment = V.dot({ 0.f, 1.f, 0.f });
  penetration = box.max[1] - Q[1];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { 0.f, 1.f, 0.f });
    maxScore = score;
    face = 3;
    contactPenetration = worldSpacePen;
  }
  // min-z (far) face
  alignment = V.dot({ 0.f, 0.f, -1.f });
  penetration = Q[2] - box.min[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { 0.f, 0.f, -1.f });
    maxScore = score;
    face = 4;
    contactPenetration = worldSpacePen;
  }
  // max-z (near) face
  alignment = V.dot({ 0.f, 0.f, 1.f });
  penetration = box.max[2] - Q[2];
  proximity = penetration / w;
  score = alignment / proximity;
  if (score > maxScore) {
    float worldSpacePen = penetration * calcScaleFactor(boxToWorldSpace, { 0.f, 0.f, 1.f });
    maxScore = score;
    face = 5;
    contactPenetration = worldSpacePen;
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

void boxXBoxPointContact(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  auto boxBToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxBSpace = inverse(boxBToWorldSpace);

  auto verts = getVertices(A.box->boundingBox, getTransform(A));
  auto invTangents = getInverseTangents(A.box->boundingBox, getTransform(A));

  for (size_t i = 0; i < 8; ++i) {
    Vec3f normal;

    float penetration = pointBoxPenetration(B.box->boundingBox, worldToBoxBSpace, boxBToWorldSpace,
      verts[i], invTangents[i], normal);

    if (penetration > 0.f) {
      Contact contact;
      contact.A = A;
      contact.B = B;
      contact.normal = normal;
      contact.penetration = penetration;
      contact.point = verts[i];
      contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
      contact.toContactSpace = contact.fromContactSpace.t();
      contacts.push_back(contact);
    }
  }
}

void boxBoxPointContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  boxXBoxPointContact(A, B, contacts);
  boxXBoxPointContact(B, A, contacts);
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

bool closestConnectingEdge(const Edge& edge1, const Edge& edge2, Edge& connecting)
{
  // Line: a + sv
  Vec3f a = edge1.A;
  Vec3f v = edge1.B - edge1.A;

  // Line: b + tw
  Vec3f b = edge2.A;
  Vec3f w = edge2.B - edge2.A;

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
    return false;
  }

  const float epsilon = 0.0001f;

  if (!inRange(st[0], 0.f - epsilon, 1.f + epsilon)) {
    return false;
  }

  if (!inRange(st[1], 0.f - epsilon, 1.f + epsilon)) {
    return false;
  }

  connecting = {
    .A = a + v * st[0],
    .B = b + w * st[1]
  };

  return true;
}

void allEdgeBoxPenetrations(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const std::array<Edge, 12>& boxEdges, const Edge& worldSpaceEdge,
  const std::function<bool(const Vec3f&, const Vec3f&)>& isPenetratingFn,
  std::vector<Contact>& contacts)
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
    return;
  }

  Vec3f contactNormal;
  Vec3f contactPoint;
  bool contactFound = false;
  int edgeIndex = -1;

  float minSqPenetration = std::numeric_limits<float>::max();

  for (size_t i = 0; i < boxEdges.size(); ++i) {
    auto& boxEdge = boxEdges[i];

    Edge connecting;
    if (!closestConnectingEdge(worldSpaceEdge, boxEdge, connecting)) {
      continue;
    }

    Vec3f& P = connecting.A;
    Vec3f& Q = connecting.B;

    auto PQ = Q - P;
    float sqPenetration = PQ.squareMagnitude();

    if (sqPenetration == 0.f) {
      continue;
    }
  
    if (!isPenetratingFn(P, Q)) {
      continue;
    }

    Contact contact;
    contact.penetration = sqrtf(sqPenetration);
    contact.point = P + PQ * 0.5f;
    contact.normal = PQ.normalise();
    contacts.push_back(contact);
  }
}

float smallestEdgeBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const std::array<Edge, 12>& boxEdges, const Edge& worldSpaceEdge,
  const std::function<bool(const Vec3f&, const Vec3f&)>& isPenetratingFn, Vec3f& point,
  Vec3f& normal)
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

  Vec3f contactNormal;
  Vec3f contactPoint;
  bool contactFound = false;
  int edgeIndex = -1;

  float minSqPenetration = std::numeric_limits<float>::max();

  for (size_t i = 0; i < boxEdges.size(); ++i) {
    auto& boxEdge = boxEdges[i];

    Edge connecting;
    if (!closestConnectingEdge(worldSpaceEdge, boxEdge, connecting)) {
      continue;
    }

    Vec3f& P = connecting.A;
    Vec3f& Q = connecting.B;

    auto PQ = Q - P;
    float sqPenetration = PQ.squareMagnitude();

    if (sqPenetration == 0.f) {
      continue;
    }

    if (!isPenetratingFn(P, Q)) {
      continue;
    }

    if (sqPenetration < minSqPenetration) {
      minSqPenetration = sqPenetration;
      contactFound = true;
      contactPoint = P + PQ * 0.5f;
      contactNormal = PQ.normalise();
      edgeIndex = i;
    }
  }

  if (contactFound) {
    normal = contactNormal;
    point = contactPoint;

    return sqrtf(minSqPenetration);
  }

  return 0.f;
}

void boxBoxEdgeContact(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  auto boxAToWorldSpace = getTransform(A) * A.box->boundingBox.transform;
  auto worldToBoxASpace = inverse(boxAToWorldSpace);

  auto boxBToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxBSpace = inverse(boxBToWorldSpace);

  auto boxAVertices = getVertices(A.box->boundingBox, getTransform(A));
  auto boxBVertices = getVertices(B.box->boundingBox, getTransform(B));

  auto boxAEdges = getEdges(boxAVertices);
  auto boxBEdges = getEdges(boxBVertices);

  auto boxACentre = calcCentre(A.box->boundingBox.min, A.box->boundingBox.max, boxAToWorldSpace);
  auto boxBCentre = calcCentre(B.box->boundingBox.min, B.box->boundingBox.max, boxBToWorldSpace);

  auto isPenetratingFn = [&](const Vec3f& P, const Vec3f& Q) {
    auto PQ = (Q - P).normalise();
    auto QP = (P - Q).normalise();
    auto QA = (boxACentre - Q).normalise();
    auto PB = (boxBCentre - P).normalise();
    return PQ.dot(PB) < 0.f && QP.dot(QA) < 0.f;
  };

  for (size_t i = 0; i < 12; ++i) {
    std::vector<Contact> newContacts;
    allEdgeBoxPenetrations(B.box->boundingBox, worldToBoxBSpace, boxBEdges, boxAEdges[i],
      isPenetratingFn, newContacts);

    // TODO: Need this?
    std::sort(newContacts.begin(), newContacts.end(), [](const Contact& A, const Contact& B) {
      return A.penetration < B.penetration;
    });

    for (size_t j = 0; j < newContacts.size(); ++j) {
      Contact& contact = newContacts[j];
      contact.A = A;
      contact.B = B;
      contact.fromContactSpace = changeOfBasisMatrix(contact.normal,
        differentVector(contact.normal));
      contact.toContactSpace = contact.fromContactSpace.t();
      contacts.push_back(contact);
    }
  }
}

void generateBoxBoxContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.box != nullptr);

  size_t n = contacts.size();
  boxBoxPointContacts(A, B, contacts);
  boxBoxEdgeContact(A, B, contacts);
}

void generateCapsuleCapsuleContacts(const ObjectComponents&, const ObjectComponents&,
  std::vector<Contact>&)
{
  //assert(A.capsule != nullptr);
  //assert(B.capsule != nullptr);

  // TODO
}

inline std::array<float, 3> calcBarycentricCoords(const Triangle& triangle, const Vec3f& P)
{
  auto [ A, B, C ] = triangle;

  auto AB = B - A;
  auto AC = C - A;
  auto AP = P - A;

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

  float APdotAB = AP.dot(AB);
  float APdotAC = AP.dot(AC);

  Vec2f st;
  [[ maybe_unused ]] bool result = solve({ ABdotAB, ABdotAC, -APdotAB },
    { ABdotAC, ACdotAC, -APdotAC }, st);
  assert(result);

  float beta = st[0];
  float gamma = st[1];
  float alpha = 1.f - gamma - beta;

  return { alpha, beta, gamma };
}

Vec3f closestPoint(const Triangle& triangle, const Vec3f& P)
{
  auto [ A, B, C ] = triangle;

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

  auto [ alpha, beta, gamma ] = calcBarycentricCoords(triangle, P);

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

  return A + AB * beta + AC * gamma;
}

bool sphereTrianglesContact(const Sphere& sphere, const std::vector<Triangle>& triangles,
  Vec3f& contact, float& maxPenetration)
{
  maxPenetration = 0.f;
  std::vector<Vec3f> points(triangles.size());
  int triangleWithMaxPenetration = -1;

  auto P = sphere.centre;
  float radius = sphere.radius;

  for (size_t i = 0; i < triangles.size(); ++i) {
    auto& triangle = triangles[i];
    float minX = std::min(std::min(triangle[0][0], triangle[1][0]), triangle[2][0]);
    float maxX = std::max(std::max(triangle[0][0], triangle[1][0]), triangle[2][0]);
    float minY = std::min(std::min(triangle[0][1], triangle[1][1]), triangle[2][1]);
    float maxY = std::max(std::max(triangle[0][1], triangle[1][1]), triangle[2][1]);
    float minZ = std::min(std::min(triangle[0][2], triangle[1][2]), triangle[2][2]);
    float maxZ = std::max(std::max(triangle[0][2], triangle[1][2]), triangle[2][2]);

    if (P[1] - radius > maxY) {
      continue;
    }

    if (P[1] + radius < minY) {
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
    contact = points[triangleWithMaxPenetration];
    return true;
  }

  return false;
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

  std::vector<Triangle> triangles;
  Vec2f p{ P[0], P[2] };
  sampler.triangles(p + Vec2f{ -radius, -radius }, p + Vec2f{ radius, radius }, triangles);

  Sphere sphere{
    .centre = P,
    .radius = radius
  };

  Vec3f Q;
  float maxPenetration = 0.f;

  if (sphereTrianglesContact(sphere, triangles, Q, maxPenetration)) {
    contact.A = A;
    contact.B = B;
    contact.point = Q;
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

  // TODO: We need to scale the capsule by object B's transform

  float radius = B.capsule->capsule.radius;
  auto centre = getTranslation(getTransform(B)) + B.capsule->capsule.translation;

  Sphere bottomSphere{
    .centre = centre + Vec3f{ 0.f, -B.capsule->capsule.height * 0.5f + radius, 0.f },
    .radius = radius
  };

  Sphere topSphere{
    .centre = centre + Vec3f{ 0.f, B.capsule->capsule.height * 0.5f - radius, 0.f },
    .radius = radius
  };

  auto vertices = getVertices(A.box->boundingBox, getTransform(A));
  auto triangles = getTriangles(vertices);

  Vec3f Q;
  float maxPenetration = 0.f;

  if (sphereTrianglesContact(bottomSphere, triangles, Q, maxPenetration)) {
    Contact contact;
    contact.A = B;
    contact.B = A;
    contact.point = Q;
    contact.normal = (bottomSphere.centre - contact.point).normalise();
    contact.penetration = maxPenetration;
    contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
    contact.toContactSpace = contact.fromContactSpace.t();
    contacts.push_back(contact);
  }
  else if (sphereTrianglesContact(topSphere, triangles, Q, maxPenetration)) {
    Contact contact;

    contact.A = B;
    contact.B = A;
    contact.point = Q;
    contact.normal = (topSphere.centre - contact.point).normalise();
    contact.penetration = maxPenetration;
    contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
    contact.toContactSpace = contact.fromContactSpace.t();
    contacts.push_back(contact);
  }

  // TODO: Check for collisions against cylindrical section
}

void SysCollisionImpl::generateBoxPolyContacts(const ObjectComponents&, const ObjectComponents&,
  std::vector<Contact>&) const
{
  //assert(A.box != nullptr);
  //auto i = m_polyhedra.find(B.entityId);
  //assert(i != m_polyhedra.end());
  //const PolyhedronData& poly = *i->second;

  // TODO
}

void SysCollisionImpl::generateCapsulePolyContacts(const ObjectComponents&,
  const ObjectComponents&, std::vector<Contact>&) const
{
  //assert(A.capsule != nullptr);
  //auto i = m_polyhedra.find(B.entityId);
  //assert(i != m_polyhedra.end());
  //const PolyhedronData& poly = *i->second;

  // TODO
}

void boxXTerrainPointContact(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  HeightMapSampler sampler{B.terrain->heightMap, getTranslation(getTransform(B))};

  auto verts = getVertices(A.box->boundingBox, getTransform(A));

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

    if (penetration > 0.f) {
      Contact contact;
      contact.A = A;
      contact.B = B;
      contact.normal = n;
      contact.point = verts[i];
      contact.fromContactSpace = changeOfBasisMatrix(contact.normal,
        differentVector(contact.normal));
      contact.toContactSpace = contact.fromContactSpace.t();
      contact.penetration = penetration;
      contacts.push_back(contact);
    }
  }
}

void terrainXBoxPointContact(const ObjectComponents& A, const ObjectComponents& B,
  const Vec2f& boxMin, const Vec2f& boxMax, std::vector<Contact>& contacts)
{
  assert(A.terrain != nullptr);
  assert(B.box != nullptr);

  HeightMapSampler sampler{A.terrain->heightMap, getTranslation(getTransform(A))};

  std::vector<Vec3f> terrainVerts;
  sampler.vertices(boxMin, boxMax, terrainVerts);

  auto boxToWorldSpace = getTransform(B) * B.box->boundingBox.transform;
  auto worldToBoxSpace = inverse(boxToWorldSpace);

  for (size_t i = 0; i < terrainVerts.size(); ++i) {
    auto& vert = terrainVerts[i];
    Vec3f normal;
    Vec3f invTangent{ 0.f, -1.f, 0.f }; // TODO: Good enough?

    float penetration = pointBoxPenetration(B.box->boundingBox, worldToBoxSpace, boxToWorldSpace,
      vert, invTangent, normal);

    if (penetration > 0.f) {
      Contact contact;
      contact.A = A;
      contact.B = B;
      contact.normal = normal;
      contact.point = terrainVerts[i];
      contact.fromContactSpace = changeOfBasisMatrix(contact.normal,
        differentVector(contact.normal));
      contact.toContactSpace = contact.fromContactSpace.t();
      contact.penetration = penetration;
      contacts.push_back(contact);
    }
  }
}

void boxTerrainPointContact(const ObjectComponents& A, const Vec2f& boxMin, const Vec2f& boxMax,
  const ObjectComponents& B, std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  boxXTerrainPointContact(A, B, contacts);
  terrainXBoxPointContact(B, A, boxMin, boxMax, contacts);
}

void boxTerrainEdgeContacts(const ObjectComponents& A, const Vec2f& boxMin, const Vec2f& boxMax,
  const ObjectComponents& B, std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.terrain != nullptr);

  const float maxPenetration = metresToWorldUnits(0.1f);  // Magic number

  HeightMapSampler sampler{B.terrain->heightMap, getTranslation(getTransform(B))};

  std::vector<Edge> terrainEdges;
  sampler.edges(boxMin, boxMax, terrainEdges);

  auto boxToWorldSpace = getTransform(A) * A.box->boundingBox.transform;
  auto worldToBoxSpace = inverse(boxToWorldSpace);

  auto boxEdges = getEdges(getVertices(A.box->boundingBox, getTransform(A)));
  auto boxCentre = calcCentre(A.box->boundingBox.min, A.box->boundingBox.max, boxToWorldSpace);

  for (size_t i = 0; i < terrainEdges.size(); ++i) {
      auto isPenetratingFn = [&sampler, boxCentre](const Vec3f& P, const Vec3f& Q) {
        auto PQ = Q - P;
        auto QP = P - Q;
        auto PB = boxCentre - P;

        if (PQ.dot(PB) >= 0.f) {
          return false;
        }

        if (PQ.dot({ 0.f, 1.f, 0.f }) >= 0.f) {
          return false;
        }

        auto [ A, B, C ] = sampler.triangle({ Q[0], Q[2] });
        auto n = (B - A).cross(C - A);
        return (Q - A).dot(n) < 0.f;
      };

      auto& edge = terrainEdges[i];


      Vec3f point;
      Vec3f normal;

      auto penetration = smallestEdgeBoxPenetration(A.box->boundingBox, worldToBoxSpace, boxEdges,
        edge, isPenetratingFn, point, normal);

      if (penetration > 0.f && penetration < maxPenetration) {
        Contact contact;
        contact.A = A;
        contact.B = B;
        contact.normal = -normal;
        contact.penetration = penetration;
        contact.point = point;
        contact.fromContactSpace = changeOfBasisMatrix(contact.normal, differentVector(contact.normal));
        contact.toContactSpace = contact.fromContactSpace.t();
        contacts.push_back(contact);
      }
   // }
  }
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

  boxTerrainPointContact(A, boxMin, boxMax, B, contacts);
  boxTerrainEdgeContacts(A, boxMin, boxMax, B, contacts);
}

void generateBoxSphereContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  assert(A.box != nullptr);
  assert(B.sphere != nullptr);

  auto fromSphereSpace = getTransform(B) * B.sphere->ovoid.transform;
  auto toSphereSpace = inverse(fromSphereSpace);

  auto vertices = getVertices(A.box->boundingBox, toSphereSpace * getTransform(A));
  auto triangles = getTriangles(vertices);

  Sphere sphere{
    .centre = { 0.f, 0.f, 0.f },
    .radius = B.sphere->ovoid.radius
  };

  Vec3f Q;
  float maxPenetration = 0.f;

  if (sphereTrianglesContact(sphere, triangles, Q, maxPenetration)) {
    Q = (fromSphereSpace * Vec4f{ Q, { 1.f }}).sub<3>();
    auto P = getTranslation(fromSphereSpace);
    auto PQ = Q - P;
    maxPenetration *= calcScaleFactor(fromSphereSpace, PQ);
    auto normal = PQ.normalise();

    auto fromContactSpace = changeOfBasisMatrix(normal, differentVector(normal));

    contacts.push_back({
      .A = A,
      .B = B,
      .point = Q,
      .normal = normal,
      .penetration = maxPenetration,
      .toContactSpace = fromContactSpace.t(),
      .fromContactSpace = fromContactSpace
    });
  }
}

// boxVertices in cylinder space
bool boxCylinderPointContact(const ObjectComponents& A, const ObjectComponents& B,
  const Mat4x4f& fromCylinderSpace, const std::array<Vec3f, 8>& boxVertices, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.cylinder != nullptr);

  float height = B.cylinder->cylinder.height;
  float radius = B.cylinder->cylinder.radius;
  float sqRadius = radius * radius;

  auto triangles = getTriangles(boxVertices);

  float smallestSqDistance = std::numeric_limits<float>::max();
  Vec3f vertOfMaxPenetration;

  for (auto& triangle : triangles) {
    for (uint32_t i = 0; i < 3; ++i) {
      Vec3f v = triangle[i];

      if (inRange(v[1], -height * 0.5f, height * 0.5f)) {
        float sqDistance = v[0] * v[0] + v[2] * v[2];

        if (sqDistance < sqRadius) {
          if (sqDistance < smallestSqDistance) {
            smallestSqDistance = sqDistance;
            vertOfMaxPenetration = v;
          }
        }
      }
    }
  }

  float maxPenetration = radius - sqrtf(smallestSqDistance);

  if (maxPenetration > 0.f) {
    Vec3f cylinderSpaceNormal{ vertOfMaxPenetration[0], 0.f, vertOfMaxPenetration[2] };
    auto normal = (fromCylinderSpace * Vec4f{ cylinderSpaceNormal, { 0.f }}).sub<3>().normalise();
    auto fromContactSpace = changeOfBasisMatrix(normal, differentVector(normal));

    contact = {
      .A = A,
      .B = B,
      .point = (fromCylinderSpace * Vec4f{ vertOfMaxPenetration, { 1.f }}).sub<3>(),
      .normal = normal,
      .penetration = maxPenetration * calcScaleFactor(fromCylinderSpace, cylinderSpaceNormal),
      .toContactSpace = fromContactSpace.t(),
      .fromContactSpace = fromContactSpace
    };

    return true;
  }

  return false;
}

bool boxCylinderEdgeContact(const ObjectComponents& A, const ObjectComponents& B,
  const Mat4x4f& fromCylinderSpace, const std::array<Vec3f, 8>& boxVertices, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.cylinder != nullptr);

  float height = B.cylinder->cylinder.height;
  float radius = B.cylinder->cylinder.radius;
  float sqRadius = radius * radius;

  // In cylinder space
  auto edges = getEdges(boxVertices);

  Edge centreLine{
    .A{ 0.f, -height * 0.5f, 0.f },
    .B{ 0.f, height * 0.5f, 0.f }
  };

  float smallestSqDistance = std::numeric_limits<float>::max();
  Vec3f pointOfMaxPenetration;
  Vec3f pqOfMaxPenetration;

  for (auto& edge : edges) {
    Edge connecting;
    if (closestConnectingEdge(centreLine, edge, connecting)) {
      auto PQ = connecting.B - connecting.A;
      auto sqDistance = PQ.squareMagnitude();

      if (sqDistance < sqRadius) {
        if (sqDistance < smallestSqDistance) {
          smallestSqDistance = sqDistance;
          pointOfMaxPenetration = connecting.B;
          pqOfMaxPenetration = PQ;
        }
      }
    }
  }

  float maxPenetration = radius - sqrtf(smallestSqDistance);

  if (maxPenetration > 0.f) {
    auto normal = (fromCylinderSpace * Vec4f{ pqOfMaxPenetration, { 0.f }}).sub<3>().normalise();
    auto fromContactSpace = changeOfBasisMatrix(normal, differentVector(normal));

    contact = {
      .A = A,
      .B = B,
      .point = (fromCylinderSpace * Vec4f{ pointOfMaxPenetration, { 1.f }}).sub<3>(),
      .normal = normal,
      .penetration = maxPenetration * calcScaleFactor(fromCylinderSpace, pqOfMaxPenetration),
      .toContactSpace = fromContactSpace.t(),
      .fromContactSpace = fromContactSpace
    };

    return true;
  }

  return false;
}

// boxVertices in cylinder space
bool boxCylinderEndsPointContact(const ObjectComponents& A, const ObjectComponents& B,
  const Mat4x4f& fromCylinderSpace, const std::array<Vec3f, 8>& boxVertices, Contact& contact)
{
  assert(A.box != nullptr);
  assert(B.cylinder != nullptr);

  float height = B.cylinder->cylinder.height;
  float radius = B.cylinder->cylinder.radius;
  float sqRadius = radius * radius;

  float maxPenetration = 0.f;
  int vertOfMaxPenetration = -1;
  bool penetratingTop = false;

  float y0 = -height * 0.5f;
  float y1 = -y0;

  for (int i = 0; i < static_cast<int>(boxVertices.size()); ++i) {
    auto& v = boxVertices[i];

    if (!inRange(v[1], y0, y1)) {
      continue;
    }

    if (v[0] * v[0] + v[2] * v[2] > sqRadius) {
      continue;
    }

    if (v[1] > 0.f) {
      float penetration = y1 - v[1];
      if (penetration > maxPenetration) {
        maxPenetration = penetration;
        vertOfMaxPenetration = i;
        penetratingTop = true;
      }
    }
    else {
      float penetration = v[1] - y0;
      if (penetration > maxPenetration) {
        maxPenetration = penetration;
        vertOfMaxPenetration = i;
        penetratingTop = false;
      }
    }
  }

  if (vertOfMaxPenetration != -1) {
    Vec4f cylinderSpaceNormal{0.f, penetratingTop ? 1.f : -1.f, 0.f, 0.f };
    auto normal = (fromCylinderSpace * cylinderSpaceNormal).sub<3>().normalise();
    auto fromContactSpace = changeOfBasisMatrix(normal, differentVector(normal));

    float scaleFactor = calcScaleFactor(fromCylinderSpace, cylinderSpaceNormal.sub<3>());

    contact = {
      .A = A,
      .B = B,
      .point = (fromCylinderSpace * Vec4f{ boxVertices[vertOfMaxPenetration], { 1.f }}).sub<3>(),
      .normal = normal,
      .penetration = maxPenetration * scaleFactor,
      .toContactSpace = fromContactSpace.t(),
      .fromContactSpace = fromContactSpace
    };

    return true;
  }

  return false;
}

void generateBoxCylinderContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& outContacts)
{
  assert(A.box != nullptr);
  assert(B.cylinder != nullptr);

  auto fromCylinderSpace = getTransform(B) * B.cylinder->cylinder.transform;
  auto toCylinderSpace = inverse(fromCylinderSpace);

  float height = B.cylinder->cylinder.height;
  float radius = B.cylinder->cylinder.radius;

  auto boxTransform = getTransform(A);
  auto vertices = getVertices(A.box->boundingBox, toCylinderSpace * boxTransform);

  std::array<Contact, 3> contacts;
  std::array<bool, 3> contactsExist;

  contactsExist[0] = boxCylinderPointContact(A, B, fromCylinderSpace, vertices, contacts[0]);
  contactsExist[1] = boxCylinderEdgeContact(A, B, fromCylinderSpace, vertices, contacts[1]);
  contactsExist[2] = boxCylinderEndsPointContact(A, B, fromCylinderSpace, vertices, contacts[2]);

  // TODO: Cylinder ends edge contacts

  if (!(contactsExist[0] || contactsExist[1] || contactsExist[2])) {
    return;
  }

  float minPenetration = std::numeric_limits<float>::max();
  size_t contactWithMinPenetration = 0;

  for (size_t i = 0; i < contacts.size(); ++i) {
    float penetration = contacts[i].penetration;

    if (contactsExist[i] && penetration < minPenetration) {
      minPenetration = penetration;
      contactWithMinPenetration = i;
    }
  }

  outContacts.push_back(contacts[contactWithMinPenetration]);

  // TODO: Detect collisions between cylinder ends and triangle face
}

void generateCapsuleSphereContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  // TODO
}

void generateCapsuleCylinderContacts(const ObjectComponents& A, const ObjectComponents& B,
  std::vector<Contact>& contacts)
{
  // TODO
}

std::vector<Contact> SysCollisionImpl::generateContacts(const std::vector<CollisionPair>& pairs)
{
  std::vector<Contact> contacts;

  for (auto& pair : pairs) {
    if (pair.A.box) {
      if (pair.B.box) {
        generateBoxBoxContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.capsule) {
        generateBoxCapsuleContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.sphere) {
        generateBoxSphereContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.cylinder) {
        generateBoxCylinderContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.collision->isPolyhedron) {
        generateBoxPolyContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.terrain) {
        generateBoxTerrainContacts(pair.A, pair.B, contacts);
      }
    }
    else if (pair.A.capsule) {
      if (pair.B.box) {
        generateBoxCapsuleContacts(pair.B, pair.A, contacts);
      }
      else if (pair.B.capsule) {
        generateCapsuleCapsuleContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.sphere) {
        generateCapsuleSphereContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.cylinder) {
        generateCapsuleCylinderContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.collision->isPolyhedron) {
        generateCapsulePolyContacts(pair.A, pair.B, contacts);
      }
      else if (pair.B.terrain) {
        generateCapsuleTerrainContacts(pair.A, pair.B, contacts);
      }
    }
    else if (pair.A.sphere) {
      if (pair.B.box) {
        generateBoxSphereContacts(pair.B, pair.A, contacts);
      }
      else if (pair.B.capsule) {
        generateCapsuleSphereContacts(pair.B, pair.A, contacts);
      }
    }
    else if (pair.A.cylinder) {
      if (pair.B.box) {
        generateBoxCylinderContacts(pair.B, pair.A, contacts);
      }
      else if (pair.B.capsule) {
        generateCapsuleCylinderContacts(pair.B, pair.A, contacts);
      }
    }
    else if (pair.A.collision->isPolyhedron) {
      if (pair.B.box) {
        generateBoxPolyContacts(pair.B, pair.A, contacts);
      }
      else if (pair.B.capsule) {
        generateCapsulePolyContacts(pair.B, pair.A, contacts);
      }
    }
    else if (pair.A.terrain) {
      if (pair.B.box) {
        generateBoxTerrainContacts(pair.B, pair.A, contacts);
      }
      else if (pair.B.capsule) {
        generateCapsuleTerrainContacts(pair.B, pair.A, contacts);
      }
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

  float rMin = 0.05f;
  float r = rMin + (1.f - rMin) / (1.f + 100.f * worldUnitsToMetres(contact.penetration));


  float margin = 0.f;

  if (A.dynamic && A.dynamic->inverseMass != 0.f) {
    float a = (A.dynamic->inverseMass / totalInvMass) * (contact.penetration * r) + margin;
    auto outDir = contact.normal;

    applyPositionDelta(A, contact.point, outDir * a);
  }

  if (B.dynamic && B.dynamic->inverseMass != 0.f) {
    float b = (B.dynamic->inverseMass / totalInvMass) * (contact.penetration * r) + margin;
    auto outDir = -contact.normal;

    applyPositionDelta(B, contact.point, outDir * b);
  }
}

// The vector { impulse[0], 0, impulse[2] } is effectively the maximum possible friction impulse.
// Here we relax the friction impulse according to the friction coefficient.
void reapplyLateralImpulse(Vec3f& impulse, float friction)
{
  float planarImpulse = sqrtf(impulse[0] * impulse[0] + impulse[2] * impulse[2]);
  if (planarImpulse > friction * impulse[1]) {
    impulse[0] *= friction;
    impulse[2] *= friction;
  }
}

void SysCollisionImpl::resolveVelocities(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  bool aIsDynamic = A.dynamic && A.dynamic->inverseMass > 0.f;
  bool bIsDynamic = B.dynamic && B.dynamic->inverseMass > 0.f;

  // Remove effect of gravity from last tick
  if (aIsDynamic) {
    A.dynamic->linearVelocity -= { 0.f, G, 0.f };
  }
  if (bIsDynamic) {
    B.dynamic->linearVelocity -= { 0.f, G, 0.f };
  }

  auto reapplyGravity = [aIsDynamic, bIsDynamic, &A, &B]() {
    if (aIsDynamic) {
      A.dynamic->linearVelocity += { 0.f, G, 0.f };
    }
    if (bIsDynamic) {
      B.dynamic->linearVelocity += { 0.f, G, 0.f };
    }
  };

  if (aIsDynamic && bIsDynamic && (A.dynamic->framesIdle == 0 || B.dynamic->framesIdle == 0)) {
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
  // TODO: Can't we just check the y component?
  if (contactSpaceClosingV.dot(contact.normal) < 0.f) {
    reapplyGravity();
    return;
  }

  float r = 0.5f * (A.collision->restitution + B.collision->restitution);
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

  float friction = 0.5f * (A.collision->friction + B.collision->friction);
  reapplyLateralImpulse(contactSpaceImpulse, friction);

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  if (A.dynamic) {
    A.dynamic->linearVelocity += impulse * A.dynamic->inverseMass;
    if (A.rotational) {
      A.rotational->angularVelocity += aI * aPointRel.cross(impulse);
    }
  }

  if (B.dynamic) {
    B.dynamic->linearVelocity += -impulse * B.dynamic->inverseMass;
    if (B.rotational) {
      B.rotational->angularVelocity += bI * bPointRel.cross(-impulse);
    }
  }

  reapplyGravity();
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

        // Tweak these idle thresholds
        const float idleMaxLinearV = 0.11f;     // Magic number
        const float idleMaxAngularV = 0.0007f;
        const size_t idleSeconds = 3;

        if (!rotationalComps.empty()) {
          if (dynamic.linearVelocity.squareMagnitude() < idleMaxLinearV &&
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
            //DBG_LOG(m_logger, STR("id " << entityIds[i]));
            //DBG_LOG(m_logger, STR("dv " << dynamic.linearVelocity.squareMagnitude()));
            //DBG_LOG(m_logger, STR("da " << rotationalComps[i].angularVelocity.squareMagnitude()));
          }
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
      }
    }
  }
}

void SysCollisionImpl::rebuildSortList()
{
  m_sortList.clear();

  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CGlobalTransform, CBoundingBox, CCollision
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localTs = group.components<CLocalTransform>();
    auto globalTs = group.components<CGlobalTransform>();
    auto boundingBoxes = group.components<CBoundingBox>();
    auto collComps = group.components<CCollision>();
    auto collBoxComps = group.components<CCollisionBox>();
    auto collDynamicComps = group.components<CCollisionDynamic>();
    auto collRotationalComps = group.components<CCollisionRotational>();
    auto collTerrainComps = group.components<CCollisionTerrain>();
    auto collCapsuleComps = group.components<CCollisionCapsule>();
    auto collSphereComps = group.components<CCollisionSphere>();
    auto collCylinderComps = group.components<CCollisionCylinder>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < entityIds.size(); ++i) {
      auto& flags = flagsComps[i].flags;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
        m_sortList.push_back({
          .entityId = entityIds[i],
          .localTransform = &localTs[i],
          .globalTransform = collDynamicComps.empty() ? &globalTs[i] : nullptr,
          .spatialFlags = &flagsComps[i],
          .aabb = &boundingBoxes[i],
          .collision = &collComps[i],
          .dynamic = collDynamicComps.empty() ? nullptr : &collDynamicComps[i],
          .rotational = collRotationalComps.empty() ? nullptr : &collRotationalComps[i],
          .box = collBoxComps.empty() ? nullptr : &collBoxComps[i],
          .capsule = collCapsuleComps.empty() ? nullptr : &collCapsuleComps[i],
          .sphere = collSphereComps.empty() ? nullptr : &collSphereComps[i],
          .cylinder = collCylinderComps.empty() ? nullptr : &collCylinderComps[i],
          .terrain = collTerrainComps.empty() ? nullptr : &collTerrainComps[i]
        });
      }
    }
  }

  auto sortFn = [](const ObjectComponents& A, const ObjectComponents& B) {
    return A.aabb->worldSpaceAabb.min[0] < B.aabb->worldSpaceAabb.min[0];
  };

  std::sort(m_sortList.begin(), m_sortList.end(), sortFn);

  m_shouldRebuildSortList = false;
}

void insertionSort(std::vector<ObjectComponents>& arr)
{
  int n = arr.size();
  for (int i = 1; i < n; ++i) {
    auto copy = arr[i];
    float value = copy.aabb->worldSpaceAabb.min[0];

    int j = i - 1;

    while (j >= 0 && value < arr[j].aabb->worldSpaceAabb.min[0]) {
      arr[j + 1] = arr[j];
      --j;
    }

    arr[j + 1] = copy;
  }
}

void SysCollisionImpl::updateSortList()
{
  insertionSort(m_sortList);
}

void SysCollisionImpl::update(Tick, const InputState&)
{
  const size_t maxIterations = 4;

  if (m_shouldRebuildSortList) {
    rebuildSortList();
  }
  else {
    updateSortList();
  }

  auto pairs = findPossibleCollisions();

  size_t i = 0;
  for (; i < maxIterations; ++i) {
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
    //m_logger.debug("Max iterations reached");
  }

  integrate();
}

void SysCollisionImpl::processEvent(const Event&)
{
  // TODO
}

} // namespace

SysCollisionPtr createSysCollision(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysCollisionImpl>(ecs, eventSystem, logger);
}

} // namespace lithic3d

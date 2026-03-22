#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/input.hpp"

namespace lithic3d
{
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

// TODO: Rationalise this
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
  CSpatialFlags* spatialFlags = nullptr;
  CCollision* collision = nullptr;
  CCollisionDynamic* dynamic = nullptr;
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

struct Edge
{
  Vec3f A;
  Vec3f B;
};

class HeightMapSampler
{
  public:
    HeightMapSampler(const HeightMap& heightMap, const Vec3f& position)
      : m_map(heightMap)
      , m_pos(position) {}

    std::array<Vec3f, 3> triangle(const Vec2f& p) const;

  private:
    const HeightMap m_map;
    Vec3f m_pos;
};

std::array<Vec3f, 3> HeightMapSampler::triangle(const Vec2f& p) const
{
  DBG_ASSERT(inRange(p[0], m_pos[0], m_pos[0] + m_map.width), "Value out of range");
  DBG_ASSERT(inRange(p[1], m_pos[1], m_pos[1] + m_map.height), "Value out of range");

  float xIdx = (p[0] - m_pos[0]) * m_map.widthPx / m_map.width;
  float zIdx = (p[1] - m_pos[1]) * m_map.heightPx / m_map.height;

  float dx = m_map.width / (m_map.widthPx - 1);
  float dz = m_map.height / (m_map.heightPx - 1);

  auto xIdx0 = floorf(xIdx);
  auto zIdx0 = floorf(zIdx);
  auto xIdx1 = ceilf(xIdx);
  auto zIdx1 = ceilf(zIdx);

  auto getVertex = [this, dx, dz](float xIdx, float zIdx) {
    size_t i = static_cast<size_t>(zIdx) * m_map.widthPx + static_cast<size_t>(xIdx);
    return Vec3f{ dx * xIdx, m_map.data.at(i), dz * zIdx };
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

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DStaticBox& data) override;
    void addEntity(EntityId id, const DDynamicBox& data) override;
    void addEntity(EntityId id, const DTerrainChunk& data) override;

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

    void applyForce(CCollisionDynamic& comp, const Force& force);
    void applyTorque(CCollisionDynamic& comp, const Force& torque);
    void applyGravity(CCollisionDynamic& comp);
    std::vector<CollisionPair> findPossibleCollisions();
    std::vector<Contact> generateContacts(const std::vector<CollisionPair>& pairs);
    void resolveContacts(const std::vector<Contact>& contacts);
    void integrate();
};

SysCollisionImpl::SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
{
}

void SysCollisionImpl::addEntity(EntityId id, const DStaticBox& data)
{
  // TODO
}

void SysCollisionImpl::addEntity(EntityId id, const DDynamicBox& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);
  assertHasComponent<CCollisionBox>(componentStore, id);
  assertHasComponent<CCollisionDynamic>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
    .restitution = data.restitution,
    .friction = data.friction
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
    .torques{},
    .angularAcceleration{},
    .angularVelocity{},
    .inverseInertialTensor = computeInverseInertialTensor(data.boundingBox, data.inverseMass)
  };

  if (dynamic.inverseMass != 0) {
    applyGravity(dynamic);
  }

  componentStore.instantiate<CCollisionDynamic>(id) = dynamic;
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  componentStore.instantiate<CCollision>(id) = CCollision{
  };
}

void SysCollisionImpl::removeEntity(EntityId entityId)
{
  // TODO
}

bool SysCollisionImpl::hasEntity(EntityId entityId) const
{
  // TODO
  return false;
}

void SysCollisionImpl::applyGravity(CCollisionDynamic& comp)
{
  if (comp.inverseMass == 0.f) {
    return;
  }

  float f = (1.f / comp.inverseMass) * G;

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
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  applyTorque(comp, {
    .force = torque,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyTorque(CCollisionDynamic& comp, const Force& torque)
{
  uint32_t i = findFreeIndex(comp.torques);
  if (i == MAX_FORCES) {
    m_logger.warn("Max torques exceeded on object");
    return;
  }
  comp.torques[i] = torque;
  comp.idle = false;
  comp.framesIdle = 0;
}

void SysCollisionImpl::setStationary(EntityId id)
{
  auto& comp = m_ecs.componentStore().component<CCollisionDynamic>(id);
  comp.linearAcceleration = {};
  comp.linearVelocity = {};
  comp.linearForces = {};
  comp.angularAcceleration = {};
  comp.angularVelocity = {};
  comp.torques = {};

  if (comp.inverseMass != 0.f) {
    applyGravity(comp);
  }
}

void SysCollisionImpl::setInverseMass(EntityId id, float inverseMass)
{
  auto& dynamic = m_ecs.componentStore().component<CCollisionDynamic>(id);
  auto& box = m_ecs.componentStore().component<CCollisionBox>(id);

  bool shouldApplyGravity = dynamic.inverseMass == 0.f && inverseMass != 0.f;

  dynamic.inverseMass = inverseMass;
  dynamic.inverseInertialTensor = computeInverseInertialTensor(box.boundingBox,
    dynamic.inverseMass);

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
    CSpatialFlags, CLocalTransform, CBoundingBox, CCollision
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localTs = group.components<CLocalTransform>();
    auto boundingBoxes = group.components<CBoundingBox>();
    auto collisionComps = group.components<CCollision>();
    auto collisionBoxComps = group.components<CCollisionBox>();
    auto collisionDynamicComps = group.components<CCollisionDynamic>();
    auto collisionTerrainComps = group.components<CCollisionTerrain>();
    auto collisionCapsuleComps = group.components<CCollisionCapsule>();
    auto entityIds = group.entityIds();

    for (size_t i = 0; i < entityIds.size(); ++i) {
      auto& flags = flagsComps[i].flags;

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)) {
        for (size_t j = i + 1; j < entityIds.size(); ++j) {
          auto& flagsJ = flagsComps[j].flags;

          if (flagsJ.test(SpatialFlags::ParentEnabled) && flagsJ.test(SpatialFlags::Enabled)) {
            auto& box1 = boundingBoxes[i].worldSpaceAabb;
            auto& box2 = boundingBoxes[j].worldSpaceAabb;

            if (box1.max[0] >= box2.min[0] && box1.min[0] <= box2.max[0] &&
              box1.max[1] >= box2.min[1] && box1.min[1] <= box2.max[1] &&
              box1.max[2] >= box2.min[2] && box1.min[2] <= box2.max[2]) {

              pairs.push_back({
                .A = {
                  .entityId = entityIds[i],
                  .localTransform = &localTs[i],
                  .spatialFlags = &flagsComps[i],
                  .collision = &collisionComps[i],
                  .dynamic = collisionDynamicComps.empty() ? nullptr : &collisionDynamicComps[i],
                  .box = collisionBoxComps.empty() ? nullptr : &collisionBoxComps[i],
                  .capsule = collisionCapsuleComps.empty() ? nullptr : &collisionCapsuleComps[i],
                  .terrain = collisionTerrainComps.empty() ? nullptr : &collisionTerrainComps[i]
                },
                .B = {
                  .entityId = entityIds[j],
                  .localTransform = &localTs[j],
                  .spatialFlags = &flagsComps[j],
                  .collision = &collisionComps[j],
                  .dynamic = collisionDynamicComps.empty() ? nullptr : &collisionDynamicComps[j],
                  .box = collisionBoxComps.empty() ? nullptr : &collisionBoxComps[j],
                  .capsule = collisionCapsuleComps.empty() ? nullptr : &collisionCapsuleComps[j],
                  .terrain = collisionTerrainComps.empty() ? nullptr : &collisionTerrainComps[j]
                },
              });
            }
          }
        }
      }
    }
  }

  return pairs;
}

inline bool isInside(const BoundingBox& box, const Vec4f& P)
{
  return
    P[0] > box.min[0] && P[0] < box.max[0] &&
    P[1] > box.min[1] && P[1] < box.max[1] &&
    P[2] > box.min[2] && P[2] < box.max[2];
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
  return {
    (transform * box.transform * Vec4f{ box.min[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // A  
    (transform * box.transform * Vec4f{ box.max[0], box.min[1], box.min[2], 1.f }).sub<3>(),  // B
    (transform * box.transform * Vec4f{ box.min[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // C
    (transform * box.transform * Vec4f{ box.max[0], box.max[1], box.min[2], 1.f }).sub<3>(),  // D
    (transform * box.transform * Vec4f{ box.min[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // E
    (transform * box.transform * Vec4f{ box.max[0], box.min[1], box.max[2], 1.f }).sub<3>(),  // F
    (transform * box.transform * Vec4f{ box.min[0], box.max[1], box.max[2], 1.f }).sub<3>(),  // G
    (transform * box.transform * Vec4f{ box.max[0], box.max[1], box.max[2], 1.f }).sub<3>()   // H
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
float pointBoxPenetration(const BoundingBox& box, const Mat4x4f& worldToBoxSpace,
  const Mat4x4f& boxToWorldSpace, const Vec3f& P, Vec3f& normal)
{
  auto Q = worldToBoxSpace * Vec4f{ P, { 1.f }};

  if (!isInside(box, Q)) {
    return 0.f;
  }

  float minPenetration = std::numeric_limits<float>::max();
  int nearestFace = 0; // 0 min-x, 1 max-x, 2 min-y, 3 max-y, 4 min-z, 5 max-z

  // min-x (left) face
  float penetration = Q[0] - box.min[0];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 0;
  }
  // max-x (right) face
  penetration = box.max[0] - Q[0];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 1;
  }
  // min-y (bottom) face
  penetration = Q[1] - box.min[1];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 2;
  }
  // max-y (top) face
  penetration = box.max[1] - Q[1];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 3;
  }
  // min-z (far) face
  penetration = Q[2] - box.min[2];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 4;
  }
  // max-z (near) face
  penetration = box.max[2] - Q[2];
  assert(penetration >= 0.f);
  if (penetration < minPenetration) {
    minPenetration = penetration;
    nearestFace = 5;
  }

  // Set w components to zero to ignore translation
  switch (nearestFace) {
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
      assert(false);
  }

  return minPenetration;
}

// Check if any of box1's vertices intersect box2 and generate a contact
bool checkForPointContacts(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2ToWorldSpace, const Mat4x4f& worldToBox2Space,
  Contact& contact)
{
  float maxPenetration = 0.f;
  int vertWithMaxPenetration = -1;
  std::array<Vec3f, 8> normals;

  auto verts = getVertices(box1, box1Transform);
  for (size_t i = 0; i < 8; ++i) {
    float penetration = pointBoxPenetration(box2, worldToBox2Space, box2ToWorldSpace, verts[i],
      normals[i]);

    if (penetration > maxPenetration) {
      vertWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (vertWithMaxPenetration == -1) {
    return false;
  }

  contact.normal = normals[vertWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = verts[vertWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, { 1.f, 1.f, 1.f });  // TODO
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

// Solve system of equations for s and t:
// l1: As + Bt + C = 0
// l2: Ds + Et + F = 0
// Returns false if no solution (lines are parallel)
inline bool solve(const Vec3f& l1, const Vec3f& l2, Vec2f& st)
{
  float determinant = l1[0] * l2[1] - l1[1] * l2[0];
  if (determinant == 0.f) {
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

    Vec3f P = a + v * st[0];

    if (!isInside(box, worldToBoxSpace * Vec4f{ P[0], P[1], P[2], 1.f })) {
      continue;
    }

    Vec3f Q = b + w * st[1];

    float sqPenetration = (Q - P).squareMagnitude();
    if (sqPenetration < minSqPenetration) {
      minSqPenetration = sqPenetration;
      indexOfMinPenetration = i;
      points[i] = P;  // TODO: Use midpoint between P and Q?
      normals[i] = (Q - P).normalise();
    }
  }

  if (indexOfMinPenetration != -1) {
    normal = normals[indexOfMinPenetration];
    point = points[indexOfMinPenetration];

    return sqrt(minSqPenetration);
  }

  return 0.f;
}

// Check if any of box1's edges intersect box2 and generate a contact
bool checkForEdgeContacts(const BoundingBox& box1, const Mat4x4f& box1Transform,
  const BoundingBox& box2, const Mat4x4f& box2ToWorldSpace, const Mat4x4f& worldToBox2Space,
  Contact& contact)
{
  float maxPenetration = 0.f;
  int edgeWithMaxPenetration = -1;
  std::array<Vec3f, 12> points;
  std::array<Vec3f, 12> normals;

  auto edges = getEdges(getVertices(box1, box1Transform));
  for (size_t i = 0; i < 12; ++i) {
    float penetration = edgeBoxPenetration(box2, worldToBox2Space, box2ToWorldSpace, edges[i],
      points[i], normals[i]);

    if (penetration > maxPenetration) {
      edgeWithMaxPenetration = i;
      maxPenetration = penetration;
    }
  }

  if (edgeWithMaxPenetration == -1) {
    return false;
  }

  contact.normal = normals[edgeWithMaxPenetration];
  contact.penetration = maxPenetration;
  contact.point = points[edgeWithMaxPenetration];
  contact.fromContactSpace = changeOfBasisMatrix(contact.normal, { 1.f, 1.f, 1.f });  // TODO
  contact.toContactSpace = contact.fromContactSpace.t();

  return true;
}

std::vector<Contact> SysCollisionImpl::generateContacts(const std::vector<CollisionPair>& pairs)
{
  std::vector<Contact> contacts;

  for (auto& pair : pairs) {
    Contact contact{};

    // TODO: Not all entities have a collision box
    assert(pair.A.box != nullptr);
    assert(pair.B.box != nullptr);

    auto boxAToWorldSpace = pair.A.localTransform->transform *
      pair.A.box->boundingBox.transform;
    auto worldToBoxASpace = inverse(boxAToWorldSpace);
    auto boxBToWorldSpace = pair.B.localTransform->transform *
      pair.B.box->boundingBox.transform;
    auto worldToBoxBSpace = inverse(boxBToWorldSpace);

    if (checkForPointContacts(pair.A.box->boundingBox, pair.A.localTransform->transform,
      pair.B.box->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contact)) {

      //DBG_LOG(m_logger, "Point contact A->B");

      contact.A = pair.A;
      contact.B = pair.B;
      contacts.push_back(contact);
    }
    else if (checkForPointContacts(pair.B.box->boundingBox,
      pair.B.localTransform->transform, pair.A.box->boundingBox, boxAToWorldSpace,
      worldToBoxASpace, contact)) {

      //DBG_LOG(m_logger, "Point contact B->A");

      contact.A = pair.B;
      contact.B = pair.A;
      contacts.push_back(contact);
    }
    else if (checkForEdgeContacts(pair.A.box->boundingBox, pair.A.localTransform->transform,
      pair.B.box->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contact)) {

      //DBG_LOG(m_logger, "Edge contact A->B");

      contact.A = pair.A;
      contact.B = pair.B;
      contacts.push_back(contact);
    }
  }

  return contacts;
}

void updateTransform(CLocalTransform& localT, CSpatialFlags& spatialFlags, const Vec3f& linearV,
  const Vec3f& angularV)
{
  auto translation = translationMatrix4x4(linearV);
  auto angle = angularV.magnitude();
  auto rotation = rotationMatrix4x4(angularV.normalise(), angle);

  auto currentOffset = getTranslation(localT.transform);

  auto t = translationMatrix4x4(currentOffset) * translation * rotation *
    translationMatrix4x4(-currentOffset) * localT.transform;

  localT.transform = t;
  spatialFlags.flags.set(SpatialFlags::Dirty);
}

// Constructs a matrix that calculates the velocity induced by a given impulse.
// Its inverse is a matrix that calculates the required impulse for a given velocity.
//
// I is the inverse inertia tensor in world space
// relativePoint is the contact point relative to the centre of mass
inline Mat3x3f velocityMatrix(float inverseMass, const Mat3x3f& I, const Vec3f& relativePoint)
{
  Mat3x3f Q = skewSymmetricMatrix(relativePoint);
  Mat3x3f M = scaleMatrix<3>(inverseMass, false);
  return (Q * -1.f) * I * Q + M;
}

void resolveInterpenetration(const Contact& contact)
{
  float totalInvMass = contact.A.dynamic->inverseMass + contact.B.dynamic->inverseMass;
  DBG_ASSERT(totalInvMass != 0.f, "Cannot collide two objects of infinite mass");

  const float margin = -G;

  auto& A = contact.A;
  auto& B = contact.B;

  float a = A.dynamic->inverseMass / totalInvMass;
  float b = B.dynamic->inverseMass / totalInvMass;
  auto da = contact.normal * a * (contact.penetration + margin);
  auto db = -contact.normal * b * (contact.penetration + margin);

  if (A.dynamic->inverseMass != 0.f) {
    Mat3x3f aI = transformTensor(A.dynamic->inverseInertialTensor, A.localTransform->transform);

    auto aOrigin = getTranslation(A.localTransform->transform);
    auto aPointRel = contact.point - (aOrigin + A.dynamic->centreOfMass);
    Mat3x3f aVelocityMatrix = velocityMatrix(A.dynamic->inverseMass, aI, aPointRel);
    Mat3x3f aImpulseMatrix = inverse(aVelocityMatrix);
    Vec3f aImpulse = aImpulseMatrix * da;
    Vec3f aLinearV = aImpulse * A.dynamic->inverseMass;
    Vec3f aAngularV = aI * aPointRel.cross(aImpulse);

    updateTransform(*A.localTransform, *A.spatialFlags, aLinearV, aAngularV);
  }

  if (B.dynamic->inverseMass != 0.f) {
    Mat3x3f bI = transformTensor(B.dynamic->inverseInertialTensor, B.localTransform->transform);

    auto bOrigin = getTranslation(B.localTransform->transform);
    auto bPointRel = contact.point - (bOrigin + B.dynamic->centreOfMass);
    Mat3x3f bVelocityMatrix = velocityMatrix(B.dynamic->inverseMass, bI, bPointRel);
    Mat3x3f bImpulseMatrix = inverse(bVelocityMatrix);
    Vec3f bImpulse = bImpulseMatrix * db;
    Vec3f bLinearV = bImpulse * B.dynamic->inverseMass;
    Vec3f bAngularV = bI * bPointRel.cross(bImpulse);

    updateTransform(*B.localTransform, *B.spatialFlags, bLinearV, bAngularV);
  }
}

void resolveVelocities(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  A.dynamic->idle = false;
  B.dynamic->idle = false;

  // TODO: Use centre of mass

  auto aOrigin = getTranslation(A.localTransform->transform);
  auto aPointRel = contact.point - (aOrigin + A.dynamic->centreOfMass);
  auto aTotalWorldSpaceV = A.dynamic->linearVelocity + A.dynamic->angularVelocity.cross(aPointRel);
  auto aContactSpaceV = contact.toContactSpace * aTotalWorldSpaceV;

  auto bOrigin = getTranslation(B.localTransform->transform);
  auto bPointRel = contact.point - (bOrigin + B.dynamic->centreOfMass);
  auto bTotalWorldSpaceV = B.dynamic->linearVelocity + B.dynamic->angularVelocity.cross(bPointRel);
  auto bContactSpaceV = contact.toContactSpace * bTotalWorldSpaceV;

  auto contactSpaceSeparatingV = bContactSpaceV - aContactSpaceV;

  float r = 0.5f * (A.collision->restitution + B.collision->restitution);
  auto desiredContactSpaceSeparatingV = { 0.f, contactSpaceSeparatingV[1] * -r, 0.f };
  auto desiredContactSpaceDv = contactSpaceSeparatingV - desiredContactSpaceSeparatingV;

  Mat3x3f aI = transformTensor(A.dynamic->inverseInertialTensor, A.localTransform->transform);
  Mat3x3f bI = transformTensor(B.dynamic->inverseInertialTensor, B.localTransform->transform);

  Mat3x3f aVelocityMatrix = velocityMatrix(A.dynamic->inverseMass, aI, aPointRel);
  Mat3x3f bVelocityMatrix = velocityMatrix(B.dynamic->inverseMass, bI, bPointRel);

  Mat3x3f velocityMatrix = contact.fromContactSpace * (aVelocityMatrix + bVelocityMatrix)
    * contact.toContactSpace;
  Mat3x3f impulseMatrix = inverse(velocityMatrix);

  Vec3f contactSpaceImpulse = impulseMatrix * desiredContactSpaceDv;

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  float planarImpulse = sqrt(impulse[0] * impulse[0] + impulse[2] * impulse[2]);
  float friction = 0.5f * (A.collision->friction + B.collision->friction);
  if (planarImpulse > friction * impulse[1]) {
    impulse[0] /= planarImpulse;
    impulse[2] /= planarImpulse;
    impulse[0] *= friction * impulse[1];
    impulse[2] *= friction * impulse[1];
  }

  A.dynamic->linearVelocity += impulse * A.dynamic->inverseMass;
  A.dynamic->angularVelocity += aI * aPointRel.cross(impulse);

  B.dynamic->linearVelocity += -impulse * B.dynamic->inverseMass;
  B.dynamic->angularVelocity += bI * bPointRel.cross(-impulse);
}

void SysCollisionImpl::resolveContacts(const std::vector<Contact>& contacts)
{
  for (auto& contact : contacts) {
    resolveInterpenetration(contact);
    resolveVelocities(contact);
  }
}

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CLocalTransform, CCollision
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto dynamicComps = group.components<CCollisionDynamic>();

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& dynamic = dynamicComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)
        && !dynamic.idle && dynamic.inverseMass != 0.f) {

        if (dynamic.linearVelocity.squareMagnitude() < G * G &&
          dynamic.angularVelocity.squareMagnitude() < 0.01f) {

          if (++dynamic.framesIdle > 10) {
            DBG_LOG(m_logger, "Setting idle");
            dynamic.idle = true;
            dynamic.framesIdle = 0;
            continue;
          }
        }

        Vec3f totalForce;
        Vec3f totalTorque;

        for (size_t f = 0; f < MAX_FORCES; ++f) {
          auto& force = dynamic.linearForces[f];
          auto& torque = dynamic.torques[f];

          if (force.lifetime > 0) {
            totalForce += force.force;
            --force.lifetime;
          }

          if (torque.lifetime > 0) {
            totalTorque += torque.force;
            --torque.lifetime;
          }
        }

        updateTransform(localT[i], flagsComps[i], dynamic.linearVelocity, dynamic.angularVelocity);

        dynamic.linearAcceleration = totalForce * dynamic.inverseMass;
        dynamic.linearVelocity += dynamic.linearAcceleration;

        dynamic.angularAcceleration = dynamic.inverseInertialTensor * totalTorque;
        dynamic.angularVelocity += dynamic.angularAcceleration;
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  const size_t MAX_ITERATIONS = 20;

  size_t i = 0;
  for (; i < MAX_ITERATIONS; ++i) {
    auto contacts = generateContacts(findPossibleCollisions());
    if (contacts.empty()) {
      break;
    }
    resolveContacts(contacts);
  }
  if (i == MAX_ITERATIONS) {
    DBG_LOG(m_logger, "Max iterations exceeded");
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

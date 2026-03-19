#include "lithic3d/sys_collision.hpp"
#include "lithic3d/logger.hpp"
#include <iostream> // TODO
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
  CCollision* collision = nullptr;
  CGlobalTransform* globalTransform = nullptr;
  CLocalTransform* localTransform = nullptr;
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

class SysCollisionImpl : public SysCollision
{
  public:
    SysCollisionImpl(Ecs& ecs, EventSystem& eventSystem, Logger& logger);

    void addEntity(EntityId id, const DCollision& data) override;
    void addEntity(EntityId id, const DTerrainChunk data) override;

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

    void applyForce(CCollision& comp, const Force& force);
    void applyTorque(CCollision& comp, const Force& torque);
    void applyGravity(CCollision& comp);
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

void SysCollisionImpl::addEntity(EntityId id, const DCollision& data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
  assertHasComponent<CSpatialFlags>(componentStore, id);
  assertHasComponent<CBoundingBox>(componentStore, id);
  assertHasComponent<CCollision>(componentStore, id);

  CCollision comp{
    .boundingBox = data.boundingBox,
    .inverseMass = data.inverseMass,
    .centreOfMass = data.centreOfMass,
    .restitution = data.restitution,
    .linearForces{},
    .linearAcceleration{},
    .linearVelocity{},
    .torques{},
    .angularAcceleration{},
    .angularVelocity{},
    .inverseInertialTensor = computeInverseInertialTensor(data.boundingBox, data.inverseMass)
  };

  if (comp.inverseMass != 0) {
    applyGravity(comp);
  }

  componentStore.instantiate<CCollision>(id) = comp;
}

void SysCollisionImpl::addEntity(EntityId id, const DTerrainChunk data)
{
  auto& componentStore = m_ecs.componentStore();

  assertHasComponent<CGlobalTransform>(componentStore, id);
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

void SysCollisionImpl::applyGravity(CCollision& comp)
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
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  applyForce(comp, {
    .force = force,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyForce(CCollision& comp, const Force& force)
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
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
  applyTorque(comp, {
    .force = torque,
    .lifetime = static_cast<uint32_t>(seconds * TICKS_PER_SECOND)
  });
}

void SysCollisionImpl::applyTorque(CCollision& comp, const Force& torque)
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
  auto& comp = m_ecs.componentStore().component<CCollision>(id);
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
  auto& comp = m_ecs.componentStore().component<CCollision>(id);

  bool shouldApplyGravity = comp.inverseMass == 0.f && inverseMass != 0.f;

  comp.inverseMass = inverseMass;
  comp.inverseInertialTensor = computeInverseInertialTensor(comp.boundingBox, comp.inverseMass);

  if (shouldApplyGravity) {
    applyGravity(comp);
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

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localTs = group.components<CLocalTransform>();
    auto globalTs = group.components<CGlobalTransform>();
    auto boundingBoxes = group.components<CBoundingBox>();
    auto collisionComps = group.components<CCollision>();
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
                  .collision = &collisionComps[i],
                  .globalTransform = &globalTs[i],
                  .localTransform = &localTs[i]
                },
                .B = {
                  .entityId = entityIds[j],
                  .collision = &collisionComps[j],
                  .globalTransform = &globalTs[j],
                  .localTransform = &localTs[j]
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

  //std::cout << "Testing edge: (" << worldSpaceEdge.A << "), (" << worldSpaceEdge.B << ")\n";

  if (
    (edge.A[0] < box.min[0] && edge.B[0] < box.min[0]) ||
    (edge.A[1] < box.min[1] && edge.B[1] < box.min[1]) ||
    (edge.A[2] < box.min[2] && edge.B[2] < box.min[2]) ||
    (edge.A[0] > box.max[0] && edge.B[0] > box.max[0]) ||
    (edge.A[1] > box.max[1] && edge.B[1] > box.max[1]) ||
    (edge.A[2] > box.max[2] && edge.B[2] > box.max[2])
  ) {
    //std::cout << "Doesn't intersect\n";
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
    //std::cout << "Box edge (" << i << "): (" << boxEdge.A << "),(" << boxEdge.B << ")\n";

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
    //std::cout << "P: " << P << "\n";

    if (!isInside(box, worldToBoxSpace * Vec4f{ P[0], P[1], P[2], 1.f })) {
      continue;
    }

    Vec3f Q = b + w * st[1];
    //std::cout << "Q: " << Q << "\n";

    float sqPenetration = (Q - P).squareMagnitude();
    if (sqPenetration < minSqPenetration) {
      minSqPenetration = sqPenetration;
      indexOfMinPenetration = i;
      points[i] = P;
      normals[i] = (Q - P).normalise();
    }
  }

  if (indexOfMinPenetration != -1) {
    normal = normals[indexOfMinPenetration];
    point = points[indexOfMinPenetration];

    //std::cout << "Index " << indexOfMinPenetration << "\n";
    //std::cout << "Penetration: " << sqrt(minSqPenetration) << "\n";
    //std::cout << "Normal: " << normal << "\n";
    //std::cout << "Point: " << point  << "\n";

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
    //std::cout << "i = " << i << "\n";
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

  //std::cout << "Building contact with edge " << edgeWithMaxPenetration << "\n";
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

    auto boxAToWorldSpace = pair.A.globalTransform->transform *
      pair.A.collision->boundingBox.transform;
    auto worldToBoxASpace = inverse(boxAToWorldSpace);
    auto boxBToWorldSpace = pair.B.globalTransform->transform *
      pair.B.collision->boundingBox.transform;
    auto worldToBoxBSpace = inverse(boxBToWorldSpace);

    if (checkForPointContacts(pair.A.collision->boundingBox, pair.A.globalTransform->transform,
      pair.B.collision->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contact)) {

      //std::cout << "Point contact A->B\n";

      contact.A = pair.A;
      contact.B = pair.B;
      contacts.push_back(contact);
    }
    else if (checkForPointContacts(pair.B.collision->boundingBox,
      pair.B.globalTransform->transform, pair.A.collision->boundingBox, boxAToWorldSpace,
      worldToBoxASpace, contact)) {

      //std::cout << "Point contact B->A\n";

      contact.A = pair.B;
      contact.B = pair.A;
      contacts.push_back(contact);
    }
    else if (checkForEdgeContacts(pair.A.collision->boundingBox, pair.A.globalTransform->transform,
      pair.B.collision->boundingBox, boxBToWorldSpace, worldToBoxBSpace, contact)) {

      //std::cout << "Edge contact A->B\n";

      contact.A = pair.A;
      contact.B = pair.B;
      contacts.push_back(contact);
    }
  }

  return contacts;
}

void updateTransform(const ObjectComponents& c, const Vec3f& linearVelocity,
  const Vec3f& angularVelocity)
{
  auto translation = translationMatrix4x4(linearVelocity);
  auto angle = angularVelocity.magnitude();
  auto rotation = rotationMatrix4x4(angularVelocity.normalise(), angle);

  auto currentOffset = getTranslation(c.globalTransform->transform);

  auto t = translationMatrix4x4(currentOffset) * translation * rotation *
    translationMatrix4x4(-currentOffset) * c.localTransform->transform;

  c.localTransform->transform = t;
  //flags.set(SpatialFlags::Dirty);
}

void resolveInterpenetration(const Contact& contact)
{
  float totalInvMass = contact.A.collision->inverseMass + contact.B.collision->inverseMass;
  DBG_ASSERT(totalInvMass != 0.f, "Cannot collide two objects of infinite mass");

  const float margin = -G;

  auto& A = contact.A;
  auto& B = contact.B;

  float a = A.collision->inverseMass / totalInvMass;
  float b = B.collision->inverseMass / totalInvMass;
  auto da = contact.normal * a * (contact.penetration + margin);
  auto db = -contact.normal * b * (contact.penetration + margin);

  auto aOrigin = getTranslation(A.globalTransform->transform);
  auto aPointRel = contact.point - aOrigin;

  auto bOrigin = getTranslation(B.globalTransform->transform);
  auto bPointRel = contact.point - bOrigin;

  if (A.collision->inverseMass != 0.f) {
    Mat3x3f aI = transformTensor(A.collision->inverseInertialTensor, A.globalTransform->transform);
    Mat3x3f aQ = skewSymmetricMatrix(aPointRel);
    Mat3x3f aM = scaleMatrix<3>(A.collision->inverseMass, false);
    Mat3x3f aVelocityMatrix = (aQ * -1.f) * aI * aQ + aM;
    Mat3x3f aImpulseMatrix = inverse(aVelocityMatrix);
    Vec3f aImpulse = aImpulseMatrix * da;
    Vec3f aLinearV = aImpulse * A.collision->inverseMass;
    Vec3f aAngularV = aI * aPointRel.cross(aImpulse);

    updateTransform(contact.A, aLinearV, aAngularV);
  }

  if (B.collision->inverseMass != 0.f) {
    Mat3x3f bI = transformTensor(B.collision->inverseInertialTensor, B.globalTransform->transform);
    Mat3x3f bQ = skewSymmetricMatrix(bPointRel);
    Mat3x3f bM = scaleMatrix<3>(B.collision->inverseMass, false);
    Mat3x3f bVelocityMatrix = (bQ * -1.f) * bI * bQ + bM;
    Mat3x3f bImpulseMatrix = inverse(bVelocityMatrix);
    Vec3f bImpulse = bImpulseMatrix * db;
    Vec3f bLinearV = bImpulse * B.collision->inverseMass;
    Vec3f bAngularV = bI * bPointRel.cross(bImpulse);

    updateTransform(contact.B, bLinearV, bAngularV);
  }

  // TODO: Set dirty flag
}

void resolveVelocities(const Contact& contact)
{
  auto& A = contact.A;
  auto& B = contact.B;

  A.collision->idle = false;
  B.collision->idle = false;

  // TODO: Use centre of mass

  auto aOrigin = getTranslation(A.globalTransform->transform);
  auto aPointRel = contact.point - aOrigin;
  auto aTotalWorldSpaceV = A.collision->linearVelocity +
    A.collision->angularVelocity.cross(aPointRel);
  auto aContactSpaceV = contact.toContactSpace * aTotalWorldSpaceV;

  auto bOrigin = getTranslation(B.globalTransform->transform);
  auto bPointRel = contact.point - bOrigin;
  auto bTotalWorldSpaceV = B.collision->linearVelocity +
    B.collision->angularVelocity.cross(bPointRel);
  auto bContactSpaceV = contact.toContactSpace * bTotalWorldSpaceV;

  auto contactSpaceSeparatingV = bContactSpaceV - aContactSpaceV;

  float r = 0.5f * (A.collision->restitution + B.collision->restitution);
  auto desiredContactSpaceSeparatingV = { 0.f, contactSpaceSeparatingV[1] * -r, 0.f };
  auto desiredContactSpaceDv = contactSpaceSeparatingV - desiredContactSpaceSeparatingV;

  Mat3x3f aI = transformTensor(A.collision->inverseInertialTensor, A.globalTransform->transform);
  Mat3x3f bI = transformTensor(B.collision->inverseInertialTensor, B.globalTransform->transform);

  Mat3x3f aQ = skewSymmetricMatrix(aPointRel);
  Mat3x3f aM = scaleMatrix<3>(A.collision->inverseMass, false);
  Mat3x3f aVelocityMatrix = (aQ * -1.f) * aI * aQ + aM;

  Mat3x3f bQ = skewSymmetricMatrix(bPointRel);
  Mat3x3f bM = scaleMatrix<3>(B.collision->inverseMass, false);
  Mat3x3f bVelocityMatrix = (bQ * -1.f) * bI * bQ + bM;

  Mat3x3f velocityMatrix = contact.fromContactSpace * (aVelocityMatrix + bVelocityMatrix)
    * contact.toContactSpace;
  Mat3x3f impulseMatrix = inverse(velocityMatrix);

  Vec3f contactSpaceImpulse = impulseMatrix * desiredContactSpaceDv;

  auto impulse = contact.fromContactSpace * contactSpaceImpulse;

  float planarImpulse = sqrt(impulse[0] * impulse[0] + impulse[2] * impulse[2]);
  float friction = 0.4f; // TODO
  if (planarImpulse > friction * impulse[1]) {
    impulse[0] /= planarImpulse;
    impulse[2] /= planarImpulse;
    impulse[0] *= friction * impulse[1];
    impulse[2] *= friction * impulse[1];
  }

  A.collision->linearVelocity += impulse * A.collision->inverseMass;
  A.collision->angularVelocity += aI * aPointRel.cross(impulse);

  B.collision->linearVelocity += -impulse * B.collision->inverseMass;
  B.collision->angularVelocity += bI * bPointRel.cross(-impulse);
}

void SysCollisionImpl::resolveContacts(const std::vector<Contact>& contacts)
{
  for (auto& contact : contacts) {
    resolveInterpenetration(contact);

    m_ecs.system<SysSpatial>().update(0, {}); // TODO: This is a hack

    resolveVelocities(contact);
  }
}

void SysCollisionImpl::integrate()
{
  auto groups = m_ecs.componentStore().components<
    CSpatialFlags, CGlobalTransform, CLocalTransform, CCollision
  >();

  for (auto& group : groups) {
    auto flagsComps = group.components<CSpatialFlags>();
    auto localT = group.components<CLocalTransform>();
    auto globalT = group.components<CGlobalTransform>();
    auto collisionComps = group.components<CCollision>();
    auto entityIds = group.entityIds(); // TODO

    for (size_t i = 0; i < flagsComps.size(); ++i) {
      auto& flags = flagsComps[i].flags;
      auto& collision = collisionComps[i];

      if (flags.test(SpatialFlags::ParentEnabled) && flags.test(SpatialFlags::Enabled)
        && !collision.idle && collision.inverseMass != 0.f) {

        if (collision.linearVelocity.squareMagnitude() < G * G &&
          collision.angularVelocity.squareMagnitude() < 0.01f) {

          if (++collision.framesIdle > 10) {
            DBG_LOG(m_logger, "Setting idle");
            collision.idle = true;
            collision.framesIdle = 0;
            continue;
          }
        }

        Vec3f totalForce;
        Vec3f totalTorque;

        for (size_t f = 0; f < MAX_FORCES; ++f) {
          auto& force = collision.linearForces[f];
          auto& torque = collision.torques[f];

          if (force.lifetime > 0) {
            totalForce += force.force;
            --force.lifetime;
          }

          if (torque.lifetime > 0) {
            totalTorque += torque.force;
            --torque.lifetime;
          }
        }

        auto translation = translationMatrix4x4(collision.linearVelocity);
        auto angle = collision.angularVelocity.magnitude();
        auto rotation = rotationMatrix4x4(collision.angularVelocity.normalise(), angle);

        auto currentOffset = getTranslation(globalT[i].transform);

        auto t = translationMatrix4x4(currentOffset) * translation * rotation *
          translationMatrix4x4(-currentOffset) * localT[i].transform;

        localT[i].transform = t;
        flags.set(SpatialFlags::Dirty);

        collision.linearAcceleration = totalForce * collision.inverseMass;
        collision.linearVelocity += collision.linearAcceleration;

        //collision.lastFrameAcceleration = collision.linearAcceleration;

        collision.angularAcceleration = collision.inverseInertialTensor * totalTorque;
        collision.angularVelocity += collision.angularAcceleration;

        //std::cout << "g = " << G << "\n";
        //std::cout << "dv " << collision.linearVelocity << "\n";
        //std::cout << "da " << collision.angularVelocity.magnitude() << "\n";
      }
    }
  }
}

void SysCollisionImpl::update(Tick tick, const InputState& inputState)
{
  const size_t MAX_ITERATIONS = 20;

  //std::cout << "\n____UPDATE____\n";

  size_t i = 0;
  for (; i < MAX_ITERATIONS; ++i) {
    //std::cout << "Iteration " << i << "\n";

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

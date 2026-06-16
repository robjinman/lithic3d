#include "lithic3d/sys_render_3d.hpp"
#include "lithic3d/renderer.hpp"
#include "lithic3d/sys_spatial.hpp"
#include "lithic3d/logger.hpp"
#include "lithic3d/camera_3d.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/utils.hpp"
#include "lithic3d/time.hpp"
#include "lithic3d/component_store.hpp"
#include "lithic3d/trace.hpp"
#include "lithic3d/model_loader.hpp"
#include "lithic3d/events.hpp"
#include "lithic3d/xml.hpp"
#include <map>
#include <cassert>
#include <unordered_set>
#include <functional>

namespace lithic3d
{

using render::Renderer;
using render::MeshPtr;
using render::MaterialPtr;
using render::MeshFeatureSet;
using render::MaterialFeatureSet;
namespace MeshFeatures = render::MeshFeatures;
using render::TexturePtr;
using render::RenderPass;

LightProjection computeLightProjection(const std::array<Vec3f, 8>& corners,
  const Vec3f& worldSpaceLightDir)
{
  Vec3f centre;
  for (size_t i = 0; i < 8; ++i) {
    centre += corners[i];
  }
  centre = centre / 8.f;

  LightProjection P;
  P.pos = centre - worldSpaceLightDir;
  P.viewMatrix = lookAt(P.pos, centre);

  constexpr float floatMax = std::numeric_limits<float>::max();
  constexpr float floatMin = std::numeric_limits<float>::lowest();

  Vec3f min{ floatMax, floatMax, floatMax };
  Vec3f max{ floatMin, floatMin, floatMin };
  for (auto& pt : corners) {
    Vec4f lightSpacePt = P.viewMatrix * Vec4f{pt, { 1.f }};
    for (uint32_t i = 0; i < 3; ++i) {
      if (lightSpacePt[i] < min[i]) {
        min[i] = lightSpacePt[i];
      }
      if (lightSpacePt[i] > max[i]) {
        max[i] = lightSpacePt[i];
      }
    }
  }

  float l = min[0];
  float r = max[0];
  float b = min[1];
  float t = max[1];
                        // Swap and negate values for z
  float n = -max[2];    // Swap because in view space we're looking down the negative-z axis
  float f = -min[2];    // Negate because the orthographic function expects positive values for n
                        // and f (when their z is negative)

  // Pull the near plane back a bit so objects that are slightly out of view can cast a shadow
  n -= metresToWorldUnits(15.f);

  P.projectionMatrix = orthographic(l, r, t, b, n, f);
  P.frustum = computeFrustumFromMatrix(P.projectionMatrix * P.viewMatrix);

  //assert(dbg_isValidFrustum(P.frustum));

  return P;
}

namespace
{

struct AnimationChannelState
{
  bool stopped = false;
  size_t frame;
};

struct AnimationState
{
  std::string animationName;
  Timer timer;
  std::vector<AnimationChannelState> channels;
  bool repeat;
  size_t channelsComplete = 0;

  bool finished()
  {
    return channelsComplete == channels.size();
  }
};

class SysRender3dImpl : public SysRender3d
{
  public:
    SysRender3dImpl(float drawDistance, const Ecs& ecs, ModelLoader& modelLoader,
      Renderer& renderer, EventSystem& eventSystem, Logger& logger);

    double frameRate() const override;

    Camera3d& camera() override;
    const Camera3d& camera() const override;

    render::Renderer& renderer() override;

    void addEntity(EntityId id, DModelPtr model) override;
    void addEntity(EntityId id, DPointLightPtr light) override;
    void addEntity(EntityId id, DDirectionalLightPtr light) override;
    void addEntity(EntityId id, DSkyboxPtr skybox) override;
    void addEntity(EntityId id, DParticleEmitterPtr particleEmitter) override;

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

    void setEntityColour(EntityId id, const Vec4f& colour) override;

    void playAnimation(EntityId entityId, const std::string& name, bool repeat) override;

    ~SysRender3dImpl() override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    std::unique_ptr<Camera3d> m_camera;
    const Ecs& m_ecs;
    ModelLoader& m_modelLoader;
    Renderer& m_renderer;
    float m_drawDistance;
    // TODO: Use component store
    std::map<EntityId, DModelPtr> m_models;
    std::map<EntityId, DPointLightPtr> m_pointLights;
    std::map<EntityId, DParticleEmitterPtr> m_particleEmitters;
    std::pair<EntityId, DDirectionalLightPtr> m_directionalLight = { NULL_ENTITY_ID, nullptr };
    std::pair<EntityId, DSkyboxPtr> m_skybox = { NULL_ENTITY_ID, nullptr };
    std::map<EntityId, AnimationState> m_animationStates;
    Mat4x4f m_metresToWorld = scaleMatrix4x4(Vec3f{ 1.f, 1.f, 1.f } * WORLD_UNITS_PER_METRE);

    using DrawFilter = std::function<bool(const Submodel&)>;

    void drawModels(const EntityIdSet& entities,
      const DrawFilter& filter = [](const Submodel&) { return true; });
    void drawSkybox();
    void drawPointLights();
    void drawDirectionalLight();
    void drawParticles();
    void doShadowPass();
    void doMainPass();
    void updateAnimations();
    size_t selectLod(float z, size_t lodLevels) const;
    std::array<LightProjection, 3> computeLightProjections(const Vec3f& worldSpaceLightDir) const;
    ComponentDataPtr constructDModel(const XmlNode& xmlNode) const;
    ComponentDataPtr constructDSkybox(const XmlNode& xmlNode) const;
    ComponentDataPtr constructDDirectionalLight(const XmlNode& xmlNode) const;
    ComponentDataPtr constructDPointLight(const XmlNode& xmlNode) const;
    ComponentDataPtr constructDParticleEmitter(const XmlNode& xmlNode) const;
};

SysRender3dImpl::SysRender3dImpl(float drawDistance, const Ecs& ecs, ModelLoader& modelLoader,
  Renderer& renderer, EventSystem& eventSystem, Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderer(renderer)
  , m_drawDistance(metresToWorldUnits(drawDistance))
{
  auto viewport = m_renderer.getViewportSize();
  float aspect = static_cast<float>(viewport[0]) / viewport[1];
  float rotation = m_renderer.getViewportRotation();
  m_camera = std::make_unique<Camera3d>(aspect, rotation, drawDistance);
}

void SysRender3dImpl::processEvent(const Event& event)
{
  if (event.name == g_strWindowResize) {
    auto& e = dynamic_cast<const EWindowResize&>(event);
    float aspect = static_cast<float>(e.width) / e.height;
    float rotation = m_renderer.getViewportRotation();
    m_camera->updateParameters(aspect, rotation);
  }
}

std::array<Vec3f, 4> frustumCrossSection(const std::array<Vec3f, 8>& corners, float percentage)
{
  return {
    corners[0] + (corners[4] - corners[0]) * percentage,
    corners[1] + (corners[5] - corners[1]) * percentage,
    corners[2] + (corners[6] - corners[2]) * percentage,
    corners[3] + (corners[7] - corners[3]) * percentage,
  };
}

std::array<LightProjection, 3>
SysRender3dImpl::computeLightProjections(const Vec3f& worldSpaceLightDir) const
{
  auto& f = m_camera->getWorldSpaceFrustum();

  std::array<Vec3f, 8> corners{
    planeIntersection(f[FrustumPlane::Near], f[FrustumPlane::Left], f[FrustumPlane::Top]),
    planeIntersection(f[FrustumPlane::Near], f[FrustumPlane::Top], f[FrustumPlane::Right]),
    planeIntersection(f[FrustumPlane::Near], f[FrustumPlane::Right], f[FrustumPlane::Bottom]),
    planeIntersection(f[FrustumPlane::Near], f[FrustumPlane::Bottom], f[FrustumPlane::Left]),
    planeIntersection(f[FrustumPlane::Far], f[FrustumPlane::Left], f[FrustumPlane::Top]),
    planeIntersection(f[FrustumPlane::Far], f[FrustumPlane::Top], f[FrustumPlane::Right]),
    planeIntersection(f[FrustumPlane::Far], f[FrustumPlane::Right], f[FrustumPlane::Bottom]),
    planeIntersection(f[FrustumPlane::Far], f[FrustumPlane::Bottom], f[FrustumPlane::Left])
  };

  std::array<Vec3f, 4> crossSection1 = frustumCrossSection(corners, 0.05f);
  std::array<Vec3f, 4> crossSection2 = frustumCrossSection(corners, 0.2f);

  std::array<Vec3f, 8> nearFrustum = {
    corners[0],
    corners[1],
    corners[2],
    corners[3],
    crossSection1[0],
    crossSection1[1],
    crossSection1[2],
    crossSection1[3]
  };

  std::array<Vec3f, 8> midFrustum = {
    crossSection1[0],
    crossSection1[1],
    crossSection1[2],
    crossSection1[3],
    crossSection2[0],
    crossSection2[1],
    crossSection2[2],
    crossSection2[3]
  };

  std::array<Vec3f, 8> farFrustum = {
    crossSection2[0],
    crossSection2[1],
    crossSection2[2],
    crossSection2[3],
    corners[4],
    corners[5],
    corners[6],
    corners[7]
  };

  return {
    computeLightProjection(nearFrustum, worldSpaceLightDir),
    computeLightProjection(midFrustum, worldSpaceLightDir),
    computeLightProjection(farFrustum, worldSpaceLightDir)
  };
}

render::Renderer& SysRender3dImpl::renderer()
{
  return m_renderer;
}

double SysRender3dImpl::frameRate() const
{
  return m_renderer.frameRate();
}

const std::string& SysRender3dImpl::name() const
{
  static const std::string name = "render_3d";
  return name;
}

void SysRender3dImpl::extractComponentSpecs(const ComponentData& data,
  std::vector<ComponentSpec>& specs) const
{
  extractSpecs<DModel>(data, specs);
  extractSpecs<DSkybox>(data, specs);
  extractSpecs<DDirectionalLight>(data, specs);
  extractSpecs<DPointLight>(data, specs);
  extractSpecs<DParticleEmitter>(data, specs);
  // ...
}

ComponentDataPtr SysRender3dImpl::constructDModel(const XmlNode& xmlModel) const
{
  auto modelFile = xmlModel.attribute("file");

  bool isInstanced = xmlModel.attribute("is_instanced") == "true";
  uint32_t maxInstances = 0;
  if (isInstanced) {
    maxInstances = std::stoul(xmlModel.attribute("max_instances"));
  }

  return std::make_unique<ComponentDataWrapper<DModel>>(DModel{
    .model = m_modelLoader.loadModelAsync(modelFile, maxInstances),
    .isInstanced = isInstanced,
    .colour = { 1.f, 1.f, 1.f, 1.f } // TODO
  });
}

ComponentDataPtr SysRender3dImpl::constructDSkybox(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysRender3dImpl::constructDDirectionalLight(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysRender3dImpl::constructDPointLight(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysRender3dImpl::constructDParticleEmitter(const XmlNode&) const
{
  // TODO
  EXCEPTION("Not implemented");
}

ComponentDataPtr SysRender3dImpl::constructComponentData(const XmlNode& xmlSysRender) const
{
  auto& xmlComp = *xmlSysRender.begin();
  if (xmlComp.name() == "model") {
    return constructDModel(xmlComp);
  }
  else if (xmlComp.name() == "skybox") {
    return constructDSkybox(xmlComp);
  }
  else if (xmlComp.name() == "directional_light") {
    return constructDDirectionalLight(xmlComp);
  }
  else if (xmlComp.name() == "point_light") {
    return constructDPointLight(xmlComp);
  }
  else if (xmlComp.name() == "particle_emitter") {
    return constructDParticleEmitter(xmlComp);
  }
  else {
    EXCEPTION("Bad component data type for render 3d system");
  }
}

ComponentDataPtr SysRender3dImpl::constructComponentDataWithModifications(const ComponentData& base,
  const XmlNode&) const
{
  // TODO

  // For now, just clone the base data and ignore modifications
  if (base.typeId() == typeid(DModel).hash_code()) {
    auto& comp = dynamic_cast<const ComponentDataWrapper<DModel>&>(base).data();
    return std::make_unique<ComponentDataWrapper<DModel>>(comp);
  }
  // TODO
  // ...
  else {
    EXCEPTION("Not implemented")
  }
}

XmlNodePtr SysRender3dImpl::componentToXml(EntityId entityId, EntityId) const
{
  // TODO: Compare with prefab

  if (!hasEntity(entityId)) {
    return nullptr;
  }

  auto xmlSysRender3d = createXmlNode("render_3d");

  auto i = m_models.find(entityId);
  if (i != m_models.end()) {
    auto& comp = *i->second;
    auto& model = m_modelLoader.getModel(comp.model.id());

    auto xmlModel = createXmlNode("model");
    xmlModel->setAttribute("is_instanced", comp.isInstanced ? "true" : "false");
    xmlModel->setAttribute("file", model.filePath);

    xmlSysRender3d->addChild(std::move(xmlModel));

    return xmlSysRender3d;
  }
  // TODO
  // ...
  else {
    return nullptr;
  }
}

void SysRender3dImpl::addEntity(EntityId id, const ComponentData& data)
{
  if (data.typeId() == typeid(DModel).hash_code()) {
    auto& render = dynamic_cast<const ComponentDataWrapper<DModel>&>(data).data();
    addEntity(id, std::make_unique<DModel>(render));
  }
  // TODO
  // ...
  else {
    EXCEPTION("Not implemented")
  }
}

void SysRender3dImpl::removeEntity(EntityId entityId)
{
  m_pointLights.erase(entityId);
  m_models.erase(entityId);
  m_animationStates.erase(entityId);
  m_particleEmitters.erase(entityId);
  if (m_skybox.first == entityId) {
    m_skybox = { NULL_ENTITY_ID, nullptr };
  }
  if (m_directionalLight.first == entityId) {
    m_directionalLight = { NULL_ENTITY_ID, nullptr };
  }
}

bool SysRender3dImpl::hasEntity(EntityId entityId) const
{
  return m_models.contains(entityId) || m_pointLights.contains(entityId) ||
    m_skybox.first == entityId || m_directionalLight.first == entityId;
}

void SysRender3dImpl::addEntity(EntityId id, DModelPtr model)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_models.insert({ id, std::move(model) });
}

void SysRender3dImpl::addEntity(EntityId id, DPointLightPtr light)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_pointLights.insert({ id, std::move(light) });
}

void SysRender3dImpl::addEntity(EntityId id, DDirectionalLightPtr light)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_directionalLight = { id, std::move(light) };
}

void SysRender3dImpl::addEntity(EntityId id, DSkyboxPtr skybox)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_skybox = { id, std::move(skybox) };
}

void SysRender3dImpl::addEntity(EntityId id, DParticleEmitterPtr particleEmitter)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_particleEmitters.insert({ id, std::move(particleEmitter) });
}

void SysRender3dImpl::setEntityColour(EntityId id, const Vec4f& colour)
{
  MAP_GET(it, m_models, id);
  it->second->colour = colour;
}

void SysRender3dImpl::playAnimation(EntityId entityId, const std::string& name, bool repeat)
{
  m_animationStates[entityId] = AnimationState{
    .animationName = name,
    .timer{},
    .channels{},
    .repeat = repeat,
    .channelsComplete = 0
  };
}

Camera3d& SysRender3dImpl::camera()
{
  return *m_camera;
}

const Camera3d& SysRender3dImpl::camera() const
{
  return *m_camera;
}

void SysRender3dImpl::drawSkybox()
{
  if (m_skybox.first != NULL_ENTITY_ID) {
    assert(m_skybox.second != nullptr);

    auto& skybox = *m_skybox.second;

    m_renderer.drawSkybox(skybox.model->lods[0].resource.id(), skybox.model->lods[0].features,
      skybox.model->material.resource.id(), skybox.model->material.features);
  }
}

size_t SysRender3dImpl::selectLod(float z, size_t lodLevels) const
{
  return std::min(static_cast<size_t>((z / m_drawDistance) * lodLevels), lodLevels - 1);
}

void SysRender3dImpl::drawModels(const EntityIdSet& entities,
  const std::function<bool(const Submodel&)>& filter)
{
  for (EntityId id : entities) {
    auto& flags = m_ecs.componentStore().component<CSpatialFlags>(id).flags;
    if (!(flags.test(SpatialFlags::Enabled) && flags.test(SpatialFlags::ParentEnabled))) {
      continue;
    }

    auto entry = m_models.find(id); // TODO: Insanely inefficient!
    if (entry == m_models.end()) {
      continue;
    }

    auto& modelData = *entry->second;
    auto& globalTransform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;
    auto& model = m_modelLoader.getModel(modelData.model.id());

    Mat4x4f m = globalTransform * m_metresToWorld;

    auto worldSpacePos = Vec4f{ getTranslation(globalTransform), { 1.f }};
    auto viewSpacePos = m_camera->getViewMatrix() * worldSpacePos;
    float z = std::max(-viewSpacePos[2], 0.f);

    for (auto& submodel : model.submodels) {
      size_t lod = selectLod(z, submodel->lods.size());

      if (filter(*submodel)) {
        if (modelData.isInstanced) {
          m_renderer.drawInstance(submodel->lods[lod].resource.id(), submodel->lods[lod].features,
            submodel->material.resource.id(), submodel->material.features, m);
        }
        else {
          if (submodel->jointTransformsDirty) {
            m_renderer.drawModel(submodel->lods[lod].resource.id(), submodel->lods[lod].features,
              submodel->material.resource.id(), submodel->material.features, modelData.colour, m,
              submodel->jointTransforms);

            submodel->jointTransformsDirty = false;
          }
          else {
            m_renderer.drawModel(submodel->lods[lod].resource.id(), submodel->lods[lod].features,
              submodel->material.resource.id(), submodel->material.features, modelData.colour, m);
          }
        }
      }
    }
  }
}

void SysRender3dImpl::doShadowPass()
{
  if (m_directionalLight.first == NULL_ENTITY_ID) {
    return;
  }

  const auto& sysSpatial = m_ecs.system<SysSpatial>();

  // TODO: Check CSpatialFlags

  const auto& transform =
    m_ecs.componentStore().component<CGlobalTransform>(m_directionalLight.first).transform;
  auto lightDir = getDirection(transform);

  auto projections = computeLightProjections(lightDir);
  for (size_t cascade = 0; cascade < projections.size(); ++cascade) {
    LightProjection& projection = projections[cascade];

    auto visible = sysSpatial.getIntersecting(projection.frustum);

    m_renderer.beginPass(render::shadowPass(cascade), projection.pos, projection.viewMatrix,
      projection.projectionMatrix);

    drawModels(visible, [](const Submodel& x) {
      // Assume flags are the same for each LOD
      return x.lods[0].features.flags.test(MeshFeatures::CastsShadow);
    });

    m_renderer.endPass();
  }
}

void SysRender3dImpl::drawPointLights()
{
  for (auto& entry : m_pointLights) {
    auto id = entry.first;
    const DPointLight& light = *entry.second;

    const auto& transform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    // TODO: Visibility check?

    m_renderer.drawPointLight(light.colour, light.ambient, light.specular, transform);

    if (light.submodels.size() > 0) {
      for (auto& submodel : light.submodels) {
        m_renderer.drawModel(submodel->lods[0].resource.id(), submodel->lods[0].features,
          submodel->material.resource.id(), submodel->material.features, { light.colour, { 1.f }},
          transform * m_metresToWorld);
      }
    }
  }
}

void SysRender3dImpl::drawParticles()
{
  for (auto& entry : m_particleEmitters) {
    auto id = entry.first;

    const auto& transform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    // TODO: Visibility check?

    m_renderer.drawParticles(entry.second->material.resource.id(), transform);
  }
}

void SysRender3dImpl::drawDirectionalLight()
{
  auto id = m_directionalLight.first;

  if (id != NULL_ENTITY_ID) {
    auto& light = *m_directionalLight.second;
    auto& transform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    m_renderer.drawDirectionalLight(light.colour, light.ambient, light.specular, transform);
  }
}

void SysRender3dImpl::doMainPass()
{
  const auto& sysSpatial = m_ecs.system<SysSpatial>();

  // TODO: Check CSpatialFlags

  auto visible = sysSpatial.getIntersecting(m_camera->getWorldSpaceFrustum());

  //static long i = 0;
  //if (i++ % 60 == 0) {
  //  std::cout << visible.size() << "\n";
  //}

  m_renderer.beginPass(RenderPass::Main, m_camera->getPosition(), m_camera->getViewMatrix(),
    m_camera->getProjectionMatrix());

  drawModels(visible);
  drawSkybox();
  drawDirectionalLight();
  drawPointLights();
  drawParticles();

  m_renderer.endPass();
}

Vec4f interpolateRotation(const Vec4f& A_, const Vec4f& B_, float t)
{
  Vec4f A = A_;
  Vec4f B = B_;

  float dotProduct = A.dot(B);
  if (dotProduct < 0.f) {
    B = -B;
    dotProduct = -dotProduct;
  }
  
  if (dotProduct > 0.9955f) {
    return (A + (B - A) * t).normalise();
  }

  float theta0 = acos(dotProduct);
  float theta = t * theta0;
  float sinTheta = sin(theta);
  float sinTheta0 = sin(theta0);

  float scaleA = cos(theta) - dotProduct * sinTheta / sinTheta0;
  float scaleB = sinTheta / sinTheta0;

  return A * scaleA + B * scaleB;
}

Vec3f interpolateTranslation(const Vec3f& A, const Vec3f& B, float t)
{
  return A + (B - A) * t;
}

Vec3f interpolateScale(const Vec3f& A, const Vec3f& B, float t)
{
  return A + (B - A) * t;
}

Transform interpolate(const Transform& A, const Transform& B, float t)
{
  Transform C;
  if (!A.rotation.has_value()) {
    C.rotation = B.rotation;
  }
  else if (!B.rotation.has_value()) {
    C.rotation = A.rotation;
  }
  else if (A.rotation.has_value() && B.rotation.has_value()) {
    C.rotation = interpolateRotation(A.rotation.value(), B.rotation.value(), t);
  }
  if (!A.translation.has_value()) {
    C.translation = B.translation;
  }
  else if (!B.translation.has_value()) {
    C.translation = A.translation;
  }
  else if (A.translation.has_value() && B.translation.has_value()) {
    C.translation = interpolateTranslation(A.translation.value(), B.translation.value(), t);
  }
  if (!A.scale.has_value()) {
    C.scale = B.scale;
  }
  else if (!B.scale.has_value()) {
    C.scale = A.scale;
  }
  else if (A.scale.has_value() && B.scale.has_value()) {
    C.scale = interpolateScale(A.scale.value(), B.scale.value(), t);
  }
  return C;
}

struct PosedJoint
{
  Transform transform;
  std::vector<size_t> children;
};

void computeAbsoluteJointTransforms(const std::vector<PosedJoint>& pose,
  std::vector<Mat4x4f>& absTransforms, const Mat4x4f& parentTransform, size_t index)
{
  absTransforms[index] = parentTransform * pose[index].transform.toMatrix();
  for (size_t i : pose[index].children) {
    computeAbsoluteJointTransforms(pose, absTransforms, absTransforms[index], i);
  }
}

// TODO: Optimise
std::vector<Mat4x4f> computeJointTransforms(const Skeleton& skeleton, const Skin& skin,
  const Animation& animation, AnimationState& state)
{
  std::vector<PosedJoint> pose(skeleton.joints.size());
  for (size_t i = 0; i < skeleton.joints.size(); ++i) {
    pose[i].children = skeleton.joints[i].children;
  }
  pose[skeleton.rootNodeIndex].transform = skeleton.joints[skeleton.rootNodeIndex].transform;

  if (state.channels.empty()) {
    state.channels.resize(animation.channels.size());
  }

  // Returns true if channel is finished
  auto advanceFrame = [](float time, const AnimationChannel& channel, size_t& frame,
    size_t numFrames) {

    // Loop until the next frame is in the future
    while (channel.timestamps[frame + 1] <= time) {
      ++frame;
      if (frame + 1 == numFrames) {
        return true;
      }
    }
    return false;
  };

  for (size_t i = 0; i < animation.channels.size(); ++i) {
    auto& channel = animation.channels[i];
    size_t& frame = state.channels[i].frame;
    auto& joint = pose[channel.jointIndex];

    if (state.channels[i].stopped) {
      joint.transform.mix(channel.transforms[frame]);
      continue;
    }

    size_t numFrames = channel.timestamps.size();
    float time = static_cast<float>(state.timer.elapsed());

    assert(frame + 1 < numFrames);

    if (advanceFrame(time, channel, frame, numFrames)) {
      state.channels[i].stopped = true;
      ++state.channelsComplete;
      joint.transform.mix(channel.transforms[frame]);
      continue;
    }
    else if (frame == 0 && time < channel.timestamps[0]) {
      joint.transform.mix(channel.transforms[frame]);
      continue;
    }

    float frameDuration = channel.timestamps[frame + 1] - channel.timestamps[frame];
    float t = (time - channel.timestamps[frame]) / frameDuration;
    assert(t >= 0.f && t <= 1.f);

    const Transform& prevTransform = channel.transforms[frame];
    const Transform& nextTransform = channel.transforms[frame + 1];

    joint.transform.mix(interpolate(prevTransform, nextTransform, t));
  }

  std::vector<Mat4x4f> absTransforms(skeleton.joints.size());
  computeAbsoluteJointTransforms(pose, absTransforms, identityMatrix<4>(),
    skeleton.rootNodeIndex);

  std::vector<Mat4x4f> finalTransforms;
  assert(skin.joints.size() == skin.inverseBindMatrices.size());
  for (size_t i = 0; i < skin.joints.size(); ++i) {
    finalTransforms.push_back(absTransforms[skin.joints[i]] * skin.inverseBindMatrices[i]);
  }

  return finalTransforms;
}

void SysRender3dImpl::updateAnimations()
{
  for (auto i = m_animationStates.begin(); i != m_animationStates.end();) {
    auto& modelData = *m_models.at(i->first);
    auto& model = m_modelLoader.getModel(modelData.model.id());
    auto& state = i->second;
    auto& animationSet = *model.animations;
    auto& animation = *animationSet.animations.at(state.animationName); // TODO: Slow?

    for (auto& submodel : model.submodels) {
      auto& skeleton = *animationSet.skeleton;
      submodel->jointTransforms = computeJointTransforms(skeleton, *submodel->skin, animation,
        state);
      submodel->jointTransformsDirty = true;
    }

    if (state.finished()) {
      if (state.repeat) {
        state = AnimationState{
          .animationName = state.animationName,
          .timer{},
          .channels{},
          .repeat = true,
          .channelsComplete = 0
        };
      }
      else {
        i = m_animationStates.erase(i++);
        //m_eventSystem.raise();
        continue;
      }
    }

    ++i;
  }
}

// TODO: Hot path. Optimise
void SysRender3dImpl::update(Tick, const InputState&)
{
  updateAnimations();

  doShadowPass();
  doMainPass();
}

SysRender3dImpl::~SysRender3dImpl()
{
  DBG_TRACE(m_logger);
}

} // namespace

SysRender3dPtr createSysRender3d(float drawDistance, const Ecs& ecs, ModelLoader& modelLoader,
  Renderer& renderer, EventSystem& eventSystem, Logger& logger)
{
  return std::make_unique<SysRender3dImpl>(drawDistance, ecs, modelLoader, renderer, eventSystem,
    logger);
}

} // namespace lithic3d

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

namespace
{

struct LightProjection
{
  Vec3f pos;
  Mat4x4f viewMatrix;
  Mat4x4f projectionMatrix;
  Frustum frustum;
};

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
  size_t channelsComplete = 0;

  bool finished()
  {
    return channelsComplete == channels.size();
  }
};

class SysRender3dImpl : public SysRender3d
{
  public:
    SysRender3dImpl(const Ecs& ecs, const ModelLoader& modelLoader, Renderer& renderer,
      Logger& logger);

    double frameRate() const override;

    Camera3d& camera() override;
    const Camera3d& camera() const override;

    render::Renderer& renderer() override;

    void addEntity(EntityId id, DModelPtr model) override;
    void addEntity(EntityId id, DPointLightPtr light) override;
    void addEntity(EntityId id, DDirectionalLightPtr light) override;
    void addEntity(EntityId id, DSkyboxPtr skybox) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    void playAnimation(EntityId entityId, const std::string& name) override;

    ~SysRender3dImpl() override;

  private:
    Logger& m_logger;
    std::unique_ptr<Camera3d> m_camera;
    const Ecs& m_ecs;
    const ModelLoader& m_modelLoader;
    Renderer& m_renderer;
    // TODO: Use component store
    std::map<EntityId, DModelPtr> m_models;
    std::map<EntityId, DPointLightPtr> m_pointLights;
    std::pair<EntityId, DDirectionalLightPtr> m_directionalLight = { NULL_ENTITY_ID, nullptr };
    std::pair<EntityId, DSkyboxPtr> m_skybox = { NULL_ENTITY_ID, nullptr };
    std::map<EntityId, AnimationState> m_animationStates;

    using DrawFilter = std::function<bool(const Submodel&)>;

    void drawModels(const EntityIdSet& entities,
      const DrawFilter& filter = [](const Submodel&) { return true; });
    void drawSkybox();
    void drawPointLights();
    void drawDirectionalLight();
    void doShadowPass();
    void doMainPass();
    void updateAnimations();
    void updateCamera();
    LightProjection computeLightProjection(const std::array<Vec3f, 8>& corners,
      const Vec3f& lightDir) const;
    std::array<LightProjection, 3> computeLightProjections(const Vec3f& worldSpaceLightDir) const;
};

SysRender3dImpl::SysRender3dImpl(const Ecs& ecs, const ModelLoader& modelLoader, Renderer& renderer,
  Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderer(renderer)
{
  auto viewport = m_renderer.getViewportSize();
  float aspect = static_cast<float>(viewport[0]) / viewport[1];
  float rotation = m_renderer.getViewportRotation();
  m_camera = std::make_unique<Camera3d>(aspect, rotation);
}

LightProjection SysRender3dImpl::computeLightProjection(const std::array<Vec3f, 8>& corners,
  const Vec3f& worldSpaceLightDir) const
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

  P.frustum[FrustumPlane::Near].normal = { 0.f, 0.f, 1.f };
  P.frustum[FrustumPlane::Near].distance = fabs(min[2]);
  P.frustum[FrustumPlane::Far].normal = { 0.f, 0.f, 1.f };
  P.frustum[FrustumPlane::Far].distance = fabs(max[2]);
  P.frustum[FrustumPlane::Left].normal = { 1.f, 0.f, 0.f };
  P.frustum[FrustumPlane::Left].distance = fabs(min[0]);
  P.frustum[FrustumPlane::Right].normal = { -1.f, 0.f, 0.f };
  P.frustum[FrustumPlane::Right].distance = fabs(max[0]);
  P.frustum[FrustumPlane::Bottom].normal = { 0.f, 1.f, 0.f };
  P.frustum[FrustumPlane::Bottom].distance = fabs(min[1]);
  P.frustum[FrustumPlane::Top].normal = { 0.f, -1.f, 0.f };
  P.frustum[FrustumPlane::Top].distance = fabs(max[1]);

  P.projectionMatrix = orthographic(min[0], max[0], max[1], min[1], min[2], max[2]);

  return P;
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

// TODO: Never called?
void SysRender3dImpl::updateCamera()
{
  auto viewport = m_renderer.getViewportSize();
  float aspect = static_cast<float>(viewport[0]) / viewport[1];
  float rotation = m_renderer.getViewportRotation();
  m_camera->updateParameters(aspect, rotation);
}

render::Renderer& SysRender3dImpl::renderer()
{
  return m_renderer;
}

double SysRender3dImpl::frameRate() const
{
  return m_renderer.frameRate();
}

void SysRender3dImpl::removeEntity(EntityId entityId)
{
  m_pointLights.erase(entityId);
  m_models.erase(entityId);
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

void SysRender3dImpl::playAnimation(EntityId entityId, const std::string& name)
{
  m_animationStates[entityId] = AnimationState{
    .animationName = name,
    .timer{},
    .channels{}
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

    m_renderer.drawSkybox(skybox.model->mesh.resource.id(), skybox.model->mesh.features,
      skybox.model->material.resource.id(), skybox.model->material.features);
  }
}

void SysRender3dImpl::drawModels(const EntityIdSet& entities,
  const std::function<bool(const Submodel&)>& filter)
{
  const Vec4f white{ 1.f, 1.f, 1.f, 1.f }; // TODO: Submodel colour?

  for (EntityId id : entities) {
    auto entry = m_models.find(id);
    if (entry == m_models.end()) {
      continue;
    }

    auto& modelData = *entry->second;
    auto& globalTransform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;
    auto& model = m_modelLoader.getModel(modelData.model.id());

    for (auto& submodel : model.submodels) {
      if (filter(*submodel)) {
        if (modelData.isInstanced) {
          m_renderer.drawInstance(submodel->mesh.resource.id(), submodel->mesh.features,
            submodel->material.resource.id(), submodel->material.features, globalTransform);
        }
        else {
          if (submodel->jointTransformsDirty) {
            m_renderer.drawModel(submodel->mesh.resource.id(), submodel->mesh.features,
              submodel->material.resource.id(), submodel->material.features, white,
              globalTransform/* * submodel->mesh.transform*/, submodel->jointTransforms);

            submodel->jointTransformsDirty = false;
          }
          else {
            m_renderer.drawModel(submodel->mesh.resource.id(), submodel->mesh.features,
              submodel->material.resource.id(), submodel->material.features, white,
              globalTransform /* * submodel->mesh.transform*/);
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
      return x.mesh.features.flags.test(MeshFeatures::CastsShadow);
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

    m_renderer.drawPointLight(light.colour, light.ambient, light.specular, transform);

    if (light.submodels.size() > 0) {
      for (auto& submodel : light.submodels) {
        m_renderer.drawModel(submodel->mesh.resource.id(), submodel->mesh.features,
          submodel->material.resource.id(), submodel->material.features, { light.colour, { 1.f }},
          transform);
      }
    }
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

  m_renderer.beginPass(RenderPass::Main, m_camera->getPosition(), m_camera->getViewMatrix(),
    m_camera->getProjectionMatrix());

  drawModels(visible);
  drawSkybox();
  drawDirectionalLight();
  drawPointLights();

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
      i = m_animationStates.erase(i++);
    }
    else {
      ++i;
    }
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

SysRender3dPtr createSysRender3d(const Ecs& ecs, const ModelLoader& modelLoader, Renderer& renderer,
  Logger& logger)
{
  return std::make_unique<SysRender3dImpl>(ecs, modelLoader, renderer, logger);
}

} // namespace lithic3d

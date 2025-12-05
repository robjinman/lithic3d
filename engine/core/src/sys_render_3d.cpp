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

Frustum frustumFromMatrix(const Mat4x4f& m)
{
  Frustum frustum;

  auto calcPlane = [](const Mat4x4f& m, size_t rowA, size_t rowB, bool add) {
    auto tmp = add ? m.row(rowA) + m.row(rowB) : m.row(rowA) - m.row(rowB);
    return Plane{
      .normal = tmp.sub<3>(),
      .distance = tmp[3]
    };
  };

  frustum[0] = calcPlane(m, 3, 0, true);    // Left plane:    row 3 + row 0
  frustum[1] = calcPlane(m, 3, 0, false);   // Right plane:   row 3 - row 0
  frustum[2] = calcPlane(m, 3, 1, false);   // Top plane:     row 3 - row 1
  frustum[3] = calcPlane(m, 3, 1, true);    // Bottom plane:  row 3 + row 1
  frustum[4] = calcPlane(m, 3, 2, true);    // Near plane:    row 3 + row 2
  frustum[5] = calcPlane(m, 3, 2, false);   // Far plane:     row 3 - row 2

  return frustum;
}

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
    void addEntity(EntityId id, DLightPtr light) override;
    void addEntity(EntityId id, DSkyboxPtr skybox) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

    void playAnimation(EntityId entityId, const std::string& name) override;

    ~SysRender3dImpl() override;

  private:
    Logger& m_logger;
    Camera3d m_camera;
    const Ecs& m_ecs;
    const ModelLoader& m_modelLoader;
    Renderer& m_renderer;
    // TODO: Use component store
    std::map<EntityId, DLightPtr> m_lights;
    std::map<EntityId, DModelPtr> m_models;
    DSkyboxPtr m_skybox;
    std::map<EntityId, AnimationState> m_animationStates;

    using DrawFilter = std::function<bool(const Submodel&)>;

    Frustum computePerspectiveFrustum() const;
    Frustum computeOrthographicFrustum(const Vec3f& viewPos, const Vec3f& viewDir,
      float zFar) const;
    void drawModels(const EntityIdSet& entities,
      const DrawFilter& filter = [](const Submodel&) { return true; });
    void drawSkybox();
    void doShadowPass();
    void doMainPass();
    void updateAnimations();
};

SysRender3dImpl::SysRender3dImpl(const Ecs& ecs, const ModelLoader& modelLoader, Renderer& renderer,
  Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_modelLoader(modelLoader)
  , m_renderer(renderer)
{
}

render::Renderer& SysRender3dImpl::renderer()
{
  return m_renderer;
}

Frustum SysRender3dImpl::computePerspectiveFrustum() const
{
  return frustumFromMatrix(m_renderer.projectionMatrix() * m_camera.getMatrix());
}

Frustum SysRender3dImpl::computeOrthographicFrustum(const Vec3f& viewPos, const Vec3f& viewDir,
  float zFar) const
{
  auto viewMatrix = lookAt(viewPos, viewPos + viewDir);
  auto projMatrix = orthographic(PIf / 2.f, PIf / 2.f, 0.f, zFar);

  return frustumFromMatrix(projMatrix * viewMatrix);
}

double SysRender3dImpl::frameRate() const
{
  return m_renderer.frameRate();
}

void SysRender3dImpl::removeEntity(EntityId entityId)
{
  m_lights.erase(entityId);
  m_models.erase(entityId);

  // TODO: Skybox?
}

bool SysRender3dImpl::hasEntity(EntityId entityId) const
{
  return m_models.contains(entityId) || m_lights.contains(entityId);
  // TODO: Skybox?
}

void SysRender3dImpl::addEntity(EntityId id, DModelPtr model)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_models.insert({ id, std::move(model) });
}

void SysRender3dImpl::addEntity(EntityId id, DLightPtr light)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_lights.insert({ id, std::move(light) });
}

void SysRender3dImpl::addEntity(EntityId id, DSkyboxPtr skybox)
{
  assertHasComponent<CGlobalTransform>(m_ecs.componentStore(), id);
  assertHasComponent<CSpatialFlags>(m_ecs.componentStore(), id);

  m_skybox = std::move(skybox);
}

void SysRender3dImpl::playAnimation(EntityId entityId, const std::string& name)
{
  auto& modelComp = *m_models.at(entityId);

  m_animationStates[entityId] = AnimationState{
    .animationName = name,
    .timer{},
    .channels{}
  };
}

Camera3d& SysRender3dImpl::camera()
{
  return m_camera;
}

const Camera3d& SysRender3dImpl::camera() const
{
  return m_camera;
}

void SysRender3dImpl::drawSkybox()
{
  if (m_skybox != nullptr) {
    //m_renderer.drawSkybox(m_skybox->model.mesh, m_skybox->model.material);
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

    for (auto& submodel : modelData.model.submodels) {
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
  // TODO: Separate pass for every shadow-casting light

  if (m_lights.empty()) {
    return;
  }

  const auto& sysSpatial = m_ecs.system<SysSpatial>();

  // TODO: Check CSpatialFlags

  const auto& firstLight = *m_lights.begin()->second;
  const auto& firstLightTransform =
    m_ecs.componentStore().component<CGlobalTransform>(m_lights.begin()->first).transform;
  auto firstLightPos = getTranslation(firstLightTransform);
  auto firstLightDir = getDirection(firstLightTransform);
  auto firstLightMatrix = lookAt(firstLightPos, firstLightPos + firstLightDir);

  auto frustum = computeOrthographicFrustum(firstLightPos, firstLightDir, firstLight.zFar);
  auto visible = sysSpatial.getIntersecting(frustum);

  m_renderer.beginPass(RenderPass::Shadow, firstLightPos, firstLightMatrix);

  drawModels(visible, [](const Submodel& x) {
    return x.mesh.features.flags.test(MeshFeatures::CastsShadow);
  });

  m_renderer.endPass();
}

void SysRender3dImpl::doMainPass()
{
  const Vec4f white{ 1.f, 1.f, 1.f, 1.f }; // TODO: Colour?
  const auto& sysSpatial = m_ecs.system<SysSpatial>();

  // TODO: Check CSpatialFlags

  auto frustum = computePerspectiveFrustum();
  auto visible = sysSpatial.getIntersecting(frustum);

  m_renderer.beginPass(RenderPass::Main, m_camera.getPosition(), m_camera.getMatrix());

  drawModels(visible);
  drawSkybox();

  for (auto& entry : m_lights) {
    auto id = entry.first;
    const DLight& light = *entry.second;

    const auto& transform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    m_renderer.drawLight(light.colour, light.ambient, light.specular, light.zFar, transform);

    if (light.submodels.size() > 0) {
      for (auto& submodel : light.submodels) {
        m_renderer.drawModel(submodel->mesh.resource.id(), submodel->mesh.features,
          submodel->material.resource.id(), submodel->material.features, white, transform);
      }
    }
  }

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
    auto& model = modelData.model;
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

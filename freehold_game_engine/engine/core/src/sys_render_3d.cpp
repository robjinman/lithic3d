#include "fge/sys_render_3d.hpp"
#include "fge/renderer.hpp"
#include "fge/sys_spatial.hpp"
#include "fge/logger.hpp"
#include "fge/camera_3d.hpp"
#include "fge/exception.hpp"
#include "fge/utils.hpp"
#include "fge/time.hpp"
#include "fge/component_store.hpp"
#include <map>
#include <cassert>
#include <unordered_set>
#include <functional>

namespace fge
{

using render::Renderer;
using render::MeshPtr;
using render::MaterialPtr;
using render::MeshFeatureSet;
using render::MaterialFeatureSet;
using render::MeshHandle;
namespace MeshFeatures = render::MeshFeatures;
using render::MaterialHandle;
using render::TexturePtr;
using render::RenderPass;

namespace
{

struct AnimationChannelState
{
  bool stopped = false;
  size_t frame;
};

struct AnimationState
{
  RenderItemId animationSet = NULL_ID;
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
    SysRender3dImpl(const Ecs& ecs, Renderer& renderer, Logger& logger);

    double frameRate() const override;

    Camera3d& camera() override;
    const Camera3d& camera() const override;

    void addEntity(EntityId id, DModelPtr model) override;
    void addEntity(EntityId id, DLightPtr light) override;
    void addEntity(EntityId id, DSkyboxPtr skybox) override;

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

    void setClearColour(const Vec4f& colour) override;

    // Animations
    //
    RenderItemId addAnimations(AnimationSetPtr animations) override;
    void removeAnimations(RenderItemId id) override;
    void playAnimation(EntityId entityId, const std::string& name) override;

    //
    // Pass through to Renderer
    // ------------------------

    // Initialisation
    //
    void compileShader(const MeshFeatureSet& meshFeatures,
      const MaterialFeatureSet& materialFeatures) override;

    // Resources
    //
    RenderItemId addTexture(TexturePtr texture) override;
    RenderItemId addNormalMap(TexturePtr texture) override;
    RenderItemId addCubeMap(std::array<TexturePtr, 6>&& textures) override;

    void removeTexture(RenderItemId id) override;
    void removeCubeMap(RenderItemId id) override;

    // Meshes
    //
    MeshHandle addMesh(MeshPtr mesh) override;
    void removeMesh(RenderItemId id) override;

    // Materials
    //
    MaterialHandle addMaterial(MaterialPtr material) override;
    void removeMaterial(RenderItemId id) override;

  private:
    Logger& m_logger;
    Camera3d m_camera;
    const Ecs& m_ecs;
    Renderer& m_renderer;
    // TODO: Use component store
    std::map<EntityId, DLightPtr> m_lights;
    std::map<EntityId, DModelPtr> m_models;
    DSkyboxPtr m_skybox;
    std::map<RenderItemId, AnimationSetPtr> m_animationSets;
    std::map<EntityId, AnimationState> m_animationStates;
    Vec4f m_clearColour;

    using DrawFilter = std::function<bool(const Submodel&)>;

    std::vector<Vec2f> computePerspectiveFrustumPerimeter(const Vec3f& viewPos,
      const Vec3f& viewDir, float_t hFov) const;
    std::vector<Vec2f> computeOrthographicFrustumPerimeter(const Vec3f& viewPos,
      const Vec3f& viewDir, float_t hFov, float_t zFar) const;
    void drawModels(const std::unordered_set<EntityId>& entities,
      const DrawFilter& filter = [](const Submodel&) { return true; });
    void drawSkybox();
    void doShadowPass();
    void doMainPass();
    void updateAnimations();
};

SysRender3dImpl::SysRender3dImpl(const Ecs& ecs, Renderer& renderer, Logger& logger)
  : m_logger(logger)
  , m_ecs(ecs)
  , m_renderer(renderer)
{
}

void SysRender3dImpl::compileShader(const MeshFeatureSet& meshFeatures,
  const MaterialFeatureSet& materialFeatures)
{
  m_renderer.compileShader(false, meshFeatures, materialFeatures);
}

void SysRender3dImpl::setClearColour(const Vec4f& colour)
{
  m_clearColour = colour;
}

// TODO: Remove
std::vector<Vec2f> SysRender3dImpl::computePerspectiveFrustumPerimeter(const Vec3f& viewPos,
  const Vec3f& viewDir, float_t hFov) const
{
  auto params = m_renderer.getViewParams();
  Vec3f A{ params.nearPlane * static_cast<float_t>(tan(0.5f * hFov)), params.nearPlane, 1 };
  Vec3f B{ params.farPlane * static_cast<float_t>(tan(0.5f * hFov)), params.farPlane, 1 };
  Vec3f C{ -B[0], B[1], 1 };
  Vec3f D{ -A[0], A[1], 1 };

  float_t a = atan2(viewDir[2], viewDir[0]) - 0.5f * PIf;

  Mat3x3f m{
    cosine(a), -sine(a), viewPos[0],
    sine(a), cosine(a), viewPos[2],
    0, 0, 1
  };

  return std::vector<Vec2f>{(m * A).sub<2>(), (m * B).sub<2>(), (m * C).sub<2>(), (m * D).sub<2>()};
}

// TODO: Remove
std::vector<Vec2f> SysRender3dImpl::computeOrthographicFrustumPerimeter(const Vec3f& viewPos,
  const Vec3f& viewDir, float_t hFov, float_t zFar) const
{
  float_t w = zFar * tan(0.5f * hFov);
  Vec3f A{ w, 0.f, 1 };
  Vec3f B{ w, zFar, 1 };
  Vec3f C{ -B[0], B[1], 1 };
  Vec3f D{ -A[0], A[1], 1 };

  float_t a = atan2(viewDir[2], viewDir[0]) - 0.5f * PIf;

  Mat3x3f m{
    cosine(a), -sine(a), viewPos[0],
    sine(a), cosine(a), viewPos[2],
    0, 0, 1
  };

  return std::vector<Vec2f>{(m * A).sub<2>(), (m * B).sub<2>(), (m * C).sub<2>(), (m * D).sub<2>()};
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
  m_models.insert({ id, std::move(model) });
}

void SysRender3dImpl::addEntity(EntityId id, DLightPtr light)
{
  m_lights.insert({ id, std::move(light) });
}

void SysRender3dImpl::addEntity(EntityId id, DSkyboxPtr skybox)
{
  m_skybox = std::move(skybox);
}

RenderItemId SysRender3dImpl::addTexture(TexturePtr texture)
{
  return m_renderer.addTexture(std::move(texture));
}

RenderItemId SysRender3dImpl::addNormalMap(TexturePtr texture)
{
  return m_renderer.addNormalMap(std::move(texture));
}

RenderItemId SysRender3dImpl::addCubeMap(std::array<TexturePtr, 6>&& textures)
{
  return m_renderer.addCubeMap(std::move(textures));
}

MaterialHandle SysRender3dImpl::addMaterial(MaterialPtr material)
{
  return m_renderer.addMaterial(std::move(material));
}

MeshHandle SysRender3dImpl::addMesh(MeshPtr mesh)
{
  auto handle = m_renderer.addMesh(std::move(mesh));


  return handle;
}

void SysRender3dImpl::removeTexture(RenderItemId id)
{
  m_renderer.removeTexture(id);
}

void SysRender3dImpl::removeCubeMap(RenderItemId id)
{
  m_renderer.removeCubeMap(id);
}

void SysRender3dImpl::removeMaterial(RenderItemId id)
{
  m_renderer.removeMaterial(id);
}

void SysRender3dImpl::removeMesh(RenderItemId id)
{
  m_renderer.removeMesh(id);
}

RenderItemId SysRender3dImpl::addAnimations(AnimationSetPtr animations)
{
  static RenderItemId nextId = 0;

  auto id = nextId++;
  m_animationSets[id] = std::move(animations);

  return id;
}

void SysRender3dImpl::removeAnimations(RenderItemId id)
{
  // TODO
}

void SysRender3dImpl::playAnimation(EntityId entityId, const std::string& name)
{
  auto& model = *m_models.at(entityId);

  m_animationStates[entityId] = AnimationState{
    .animationSet = model.animations,
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
    m_renderer.drawSkybox(m_skybox->model.mesh, m_skybox->model.material);
  }
}

void SysRender3dImpl::drawModels(const std::unordered_set<EntityId>& entities,
  const std::function<bool(const Submodel&)>& filter)
{
  const Vec4f white{ 1.f, 1.f, 1.f, 1.f }; // TODO: Submodel colour?

  for (EntityId id : entities) {
    auto entry = m_models.find(id);
    if (entry == m_models.end()) {
      continue;
    }

    auto& model = *entry->second;
    auto& globalTransform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    for (auto& submodel : model.submodels) {
      if (filter(submodel)) {
        if (model.isInstanced) {
          m_renderer.drawInstance(submodel.mesh, submodel.material, globalTransform);
        }
        else {
          if (submodel.jointTransformsDirty) {
            m_renderer.drawModel(submodel.mesh, submodel.material, white,
              globalTransform * submodel.mesh.transform, submodel.jointTransforms);
            submodel.jointTransformsDirty = false;
          }
          else {
            m_renderer.drawModel(submodel.mesh, submodel.material, white,
              globalTransform * submodel.mesh.transform);
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

  const auto& firstLight = *m_lights.begin()->second;
  const auto& firstLightTransform =
    m_ecs.componentStore().component<CGlobalTransform>(m_lights.begin()->first).transform;
  auto firstLightPos = getTranslation(firstLightTransform);
  auto firstLightDir = getDirection(firstLightTransform);
  auto firstLightMatrix = lookAt(firstLightPos, firstLightPos + firstLightDir);

  auto frustum = computeOrthographicFrustumPerimeter(firstLightPos, firstLightDir,
    degreesToRadians(90.f), firstLight.zFar);
  auto visible = sysSpatial.getIntersecting(frustum); // TODO: Replace

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

  auto frustum = computePerspectiveFrustumPerimeter(m_camera.getPosition(), m_camera.getDirection(),
    m_renderer.getViewParams().hFov);
  auto visible = sysSpatial.getIntersecting(frustum); // TODO: Replace

  m_renderer.beginPass(RenderPass::Main, m_camera.getPosition(), m_camera.getMatrix());

  drawModels(visible);

  for (auto& entry : m_lights) {
    auto id = entry.first;
    const DLight& light = *entry.second;

    const auto& transform = m_ecs.componentStore().component<CGlobalTransform>(id).transform;

    m_renderer.drawLight(light.colour, light.ambient, light.specular, light.zFar, transform);

    if (light.submodels.size() > 0) {
      for (auto& submodel : light.submodels) {
        m_renderer.drawModel(submodel.mesh, submodel.material, white, transform);
      }
    }
  }

  m_renderer.endPass();
}

Vec4f interpolateRotation(const Vec4f& A_, const Vec4f& B_, float_t t)
{
  Vec4f A = A_;
  Vec4f B = B_;

  float_t dotProduct = A.dot(B);
  if (dotProduct < 0.f) {
    B = -B;
    dotProduct = -dotProduct;
  }
  
  if (dotProduct > 0.9955f) {
    return (A + (B - A) * t).normalise();
  }

  float_t theta0 = acos(dotProduct);
  float_t theta = t * theta0;
  float_t sinTheta = sin(theta);
  float_t sinTheta0 = sin(theta0);

  float_t scaleA = cos(theta) - dotProduct * sinTheta / sinTheta0;
  float_t scaleB = sinTheta / sinTheta0;

  return A * scaleA + B * scaleB;
}

Vec3f interpolateTranslation(const Vec3f& A, const Vec3f& B, float_t t)
{
  return A + (B - A) * t;
}

Vec3f interpolateScale(const Vec3f& A, const Vec3f& B, float_t t)
{
  return A + (B - A) * t;
}

Transform interpolate(const Transform& A, const Transform& B, float_t t)
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
  auto advanceFrame = [](float_t time, const AnimationChannel& channel, size_t& frame,
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
    float_t time = static_cast<float_t>(state.timer.elapsed());

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

    float_t frameDuration = channel.timestamps[frame + 1] - channel.timestamps[frame];
    float_t t = (time - channel.timestamps[frame]) / frameDuration;
    assert(t >= 0.f && t <= 1.f);

    const Transform& prevTransform = channel.transforms[frame];
    const Transform& nextTransform = channel.transforms[frame + 1];

    joint.transform.mix(interpolate(prevTransform, nextTransform, t));
  }

  std::vector<Mat4x4f> absTransforms(skeleton.joints.size());
  computeAbsoluteJointTransforms(pose, absTransforms, identityMatrix<float_t, 4>(),
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
    auto& model = *m_models.at(i->first);
    auto& state = i->second;
    auto& animationSet = *m_animationSets.at(state.animationSet);
    auto& animation = *animationSet.animations.at(state.animationName); // TODO: Slow?

    for (auto& submodel : model.submodels) {
      auto& skeleton = *animationSet.skeleton;
      submodel.jointTransforms = computeJointTransforms(skeleton, *submodel.skin, animation, state);
      submodel.jointTransformsDirty = true;
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
  try {
    updateAnimations();

    //m_renderer.beginFrame(m_clearColour);

    //doShadowPass();
    //doMainPass();

    //m_renderer.endFrame();
    //m_renderer.checkError();
  }
  catch(const std::exception& e) {
    EXCEPTION(STR("Error rendering scene; " << e.what()));
  }
}

} // namespace

SysRender3dPtr createSysRender3d(const Ecs& ecs, Renderer& renderer, Logger& logger)
{
  return std::make_unique<SysRender3dImpl>(ecs, renderer, logger);
}

} // namespace fge

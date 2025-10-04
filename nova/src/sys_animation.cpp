#include "sys_animation.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
#include <chrono>
#include <map>
#include <cassert>
#include <queue>

namespace
{

struct QueuedAnimation
{
  HashedString name = 0;
  bool repeats = false;
  std::optional<std::function<void()>> onFinish;
};

struct AnimationState
{
  AnimationId id = 0;
  HashedString name = 0;
  Tick tick = 0;
  Mat4x4f initialT;
  Vec4f startColour;
  bool repeats = false;
  std::optional<std::function<void()>> onFinish;
  std::optional<QueuedAnimation> next;
};

struct CAnimation
{
  std::map<HashedString, AnimationId> animations;
};

using CAnimationPtr = std::unique_ptr<CAnimation>;

class SysAnimationImpl : public SysAnimation
{
  public:
    SysAnimationImpl(ComponentStore& componentStore, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event&) override {}

    void addEntity(EntityId entityId, const AnimationData& data) override;
    AnimationId addAnimation(AnimationPtr animation) override;
    void replaceAnimation(AnimationId animationId, AnimationPtr animation) override;

    void playAnimation(EntityId entityId, HashedString name, bool repeat) override;
    void playAnimation(EntityId entityId, HashedString name,
      const std::function<void()>& onFinish) override;

    void queueAnimation(EntityId entityId, HashedString name, bool repeat) override;
    void queueAnimation(EntityId entityId, HashedString name,
      const std::function<void()>& onFinish) override;

    void stopAnimation(EntityId entityId) override;
    void seek(EntityId entityId, Tick tick) override;
    bool hasAnimation(EntityId entityId, HashedString name) const override;
    bool hasAnimationPlaying(EntityId entityId) const override;
    void finishAnimation(EntityId entityId) override;

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    std::map<EntityId, CAnimationPtr> m_components;
    std::map<EntityId, AnimationState> m_activeAnimations;
    std::map<AnimationId, AnimationPtr> m_animations;
    std::queue<std::function<void()>> m_callbacks;

    static AnimationId m_nextId;

    bool updateAnimation(EntityId entityId, AnimationState& animState,
      std::vector<std::tuple<EntityId, QueuedAnimation>>& queue);

    void playAnimation(EntityId entityId, HashedString name, bool repeat,
      const std::optional<std::function<void()>>& onFinish);

    void queueAnimation(EntityId entityId, HashedString name, bool repeat,
      const std::optional<std::function<void()>>& onFinish);

    void playQueuedAnimation(EntityId entityId, const QueuedAnimation& anim);
};

AnimationId SysAnimationImpl::m_nextId = 0;

SysAnimationImpl::SysAnimationImpl(ComponentStore& componentStore, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
{
}

// Returns true if animation is complete. Does not erase from m_activeAnimations.
// If the animation finishes during this frame, an animation may be added to the queue, if:
//   * The finishing animation repeats
//   * The finishing animation has a queued animation - this takes precedence over a repeating
//     animation
bool SysAnimationImpl::updateAnimation(EntityId entityId, AnimationState& animState,
  std::vector<std::tuple<EntityId, QueuedAnimation>>& queue)
{
  auto& anim = *m_animations.at(animState.id);
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  auto& localTComp = m_componentStore.component<CLocalTransform>(entityId);

  size_t numFrames = anim.frames.size();
  Tick elapsed = animState.tick;
  float_t fractionComplete = static_cast<float_t>(elapsed) / anim.duration;
  float_t frameNumFloat = fractionComplete * numFrames;
  size_t frameNumInt = static_cast<size_t>(frameNumFloat);

  ++animState.tick;

  assert(numFrames > 0);
  assert(frameNumInt <= numFrames);

  if (frameNumInt == numFrames) {
    auto& frame = anim.frames[frameNumInt - 1];

    Vec3f pos{ frame.pos[0], frame.pos[1], 0.f };
    Vec3f scale{ frame.scale[0], frame.scale[1], 1.f };
    float_t rot = degreesToRadians(frame.rotation);

    Vec3f pivot{ frame.scale[0] * frame.pivot[0], frame.scale[1] * frame.pivot[1], 0.f };

    localTComp.transform =
      // Adjust position in world space
      translationMatrix4x4(pos) *
      // Move into world space
      animState.initialT *
      // Rotation and scale in model space
      translationMatrix4x4(pivot) *
      rotationMatrix4x4(Vec3f{ 0.f, 0.f, rot }) *
      translationMatrix4x4(-pivot) *
      scaleMatrix4x4(scale);

    renderComp.colour = frame.colour;

    if (animState.repeats && !animState.next.has_value()) {
      localTComp.transform = animState.initialT;
      queue.push_back({ entityId, QueuedAnimation{
        .name = anim.name,
        .repeats = true,
        .onFinish = std::nullopt
      }});
    }
    else if (animState.next.has_value()) {
      auto& next = animState.next.value();
      queue.push_back({ entityId, QueuedAnimation{
        .name = next.name,
        .repeats = next.repeats,
        .onFinish = next.onFinish
      }});
    }

    return true;
  }

  float_t fractionOfFrameComplete = frameNumFloat - frameNumInt;
  auto& frame = anim.frames[frameNumInt];

  Vec2f prevPos = frameNumInt > 0 ? anim.frames[frameNumInt - 1].pos : anim.startPos;
  Vec2f delta = frame.pos - prevPos;
  Vec2f interp = prevPos + delta * fractionOfFrameComplete;

  Vec3f pos{ interp[0], interp[1], 0.f };
  Vec3f scale{ frame.scale[0], frame.scale[1], 1.f }; // TODO: interpolate?

  float_t prevRot = frameNumInt > 0 ? anim.frames[frameNumInt - 1].rotation : 0.f;
  float_t rotDelta = frame.rotation - prevRot;
  float_t rot = degreesToRadians(prevRot + fractionOfFrameComplete * rotDelta);

  Vec3f pivot{ frame.scale[0] * frame.pivot[0], frame.scale[1] * frame.pivot[1], 0.f };

  localTComp.transform =
    // Adjust position in world space
    translationMatrix4x4(pos) *
    // Move into world space
    animState.initialT *
    // Rotation and scale in model space
    translationMatrix4x4(pivot) *
    rotationMatrix4x4(Vec3f{ 0.f, 0.f, rot }) *
    translationMatrix4x4(-pivot) *
    scaleMatrix4x4(scale);

  if (frame.textureRect.has_value()) {
    auto& spriteComp = m_componentStore.component<CSprite>(entityId);
    spriteComp.textureRect = frame.textureRect.value();
  }

  auto& prevColour = frameNumInt > 0 ? anim.frames[frameNumInt - 1].colour : animState.startColour;
  auto& colour = frame.colour;

  renderComp.colour = prevColour * (1.f - fractionOfFrameComplete)
    + colour * fractionOfFrameComplete;

  return false;
}

void SysAnimationImpl::update(Tick, const InputState&)
{
  while (!m_callbacks.empty()) {
    m_callbacks.front()();
    m_callbacks.pop();
  }

  std::vector<std::tuple<EntityId, QueuedAnimation>> queue;

  for (auto i = m_activeAnimations.begin(); i != m_activeAnimations.end();) {
    auto entityId = i->first;
    auto& animState = i->second;

    auto& flags = m_componentStore.component<CSpatialFlags>(entityId);
    if (!(flags.enabled && flags.parentEnabled)) {
      ++i;
      continue;
    }

    if (!updateAnimation(entityId, animState, queue)) {
      ++i;
    }
    else {
      auto onFinish = animState.onFinish;
      i = m_activeAnimations.erase(i);

      if (onFinish.has_value()) {
        m_callbacks.push(onFinish.value());
      }
    }
  }

  for (auto& anim : queue) {
    playQueuedAnimation(std::get<0>(anim), std::get<1>(anim));
  }
}

void SysAnimationImpl::playQueuedAnimation(EntityId entityId, const QueuedAnimation& anim)
{
  if (anim.onFinish.has_value()) {
    playAnimation(entityId, anim.name, anim.onFinish.value());
  }
  else {
    playAnimation(entityId, anim.name, anim.repeats);
  }
}

void SysAnimationImpl::stopAnimation(EntityId entityId)
{
  auto i = m_activeAnimations.find(entityId);
  if (i == m_activeAnimations.end()) {
    return;
  }

  // Call callback on stop?
  if (i->second.onFinish.has_value()) {
    m_callbacks.push(i->second.onFinish.value());
  }

  m_activeAnimations.erase(i);
}

void SysAnimationImpl::finishAnimation(EntityId entityId)
{
  auto i = m_activeAnimations.find(entityId);
  if (i == m_activeAnimations.end()) {
    EXCEPTION("Entity has no animations playing");
  }

  auto anim = *m_animations.at(i->second.id);
  assert(anim.duration > 0);

  seek(entityId, anim.duration);

  bool hasQueuedAnimation = i->second.next.has_value();

  std::vector<std::tuple<EntityId, QueuedAnimation>> toRepeat;
  auto onFinish = i->second.onFinish;
  assert(updateAnimation(entityId, i->second, toRepeat));
  m_activeAnimations.erase(entityId);

  if (onFinish.has_value()) {
    m_callbacks.push(onFinish.value());
  }

  // Only play queued animations, not repeating animations
  if (hasQueuedAnimation) {
    for (auto& anim : toRepeat) {
      playQueuedAnimation(std::get<0>(anim), std::get<1>(anim));
    }
  }
}

void SysAnimationImpl::addEntity(EntityId entityId, const AnimationData& comp)
{
  auto data = std::make_unique<CAnimation>();

  for (auto animId : comp.animations) {
    auto& anim = *m_animations.at(animId);
    data->animations.insert({ anim.name, animId });
  }

  m_components.insert({ entityId, std::move(data) });
}

void SysAnimationImpl::removeEntity(EntityId entityId)
{
  m_activeAnimations.erase(entityId);
  m_components.erase(entityId);
}

bool SysAnimationImpl::hasEntity(EntityId entityId) const
{
  return m_components.contains(entityId);
}

AnimationId SysAnimationImpl::addAnimation(AnimationPtr animation)
{
  auto id = m_nextId++;

  ASSERT(animation->duration > 0, "Animation must have duration > 0");
  m_animations.insert({ id, std::move(animation) });

  return id;
}

void SysAnimationImpl::replaceAnimation(AnimationId animationId, AnimationPtr animation)
{
  ASSERT(animation->duration > 0, "Animation must have duration > 0");
  m_animations.erase(animationId);
  m_animations.insert({ animationId, std::move(animation) });
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name, bool repeat)
{
  playAnimation(entityId, name, repeat, std::nullopt);
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name,
  const std::function<void()>& onFinish)
{
  playAnimation(entityId, name, false, onFinish);
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name, bool repeat,
  const std::optional<std::function<void()>>& onFinish)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  auto& localTComp = m_componentStore.component<CLocalTransform>(entityId);
  auto& animComp = *m_components.at(entityId);
  auto animId = animComp.animations.at(name);

  ASSERT(!m_activeAnimations.contains(entityId), "Entity already has animation playing");

  m_activeAnimations.insert({ entityId, AnimationState{
    .id = animId,
    .name = name,
    .tick = 0,
    .initialT = localTComp.transform,
    .startColour = renderComp.colour,
    .repeats = repeat,
    .onFinish = onFinish,
    .next = std::nullopt
  }});
}

void SysAnimationImpl::queueAnimation(EntityId entityId, HashedString name, bool repeat)
{
  queueAnimation(entityId, name, repeat, std::nullopt);
}

void SysAnimationImpl::queueAnimation(EntityId entityId, HashedString name,
  const std::function<void()>& onFinish)
{
  queueAnimation(entityId, name, false, onFinish);
}

void SysAnimationImpl::queueAnimation(EntityId entityId, HashedString name, bool repeat,
  const std::optional<std::function<void()>>& onFinish)
{
  auto i = m_activeAnimations.find(entityId);

  if (i == m_activeAnimations.end()) {
    playAnimation(entityId, name, repeat, onFinish);
    return;
  }

  i->second.next = QueuedAnimation{
    .name = name,
    .repeats = repeat,
    .onFinish = onFinish
  };
}

void SysAnimationImpl::seek(EntityId entityId, Tick tick)
{
  auto i = m_activeAnimations.find(entityId);
  if (i == m_activeAnimations.end()) {
    EXCEPTION("Entity doesn't have animation playing");
  }

  auto& animState = i->second;
  auto& anim = *m_animations.at(animState.id);

  animState.tick = tick % (anim.duration + 1);
}

bool SysAnimationImpl::hasAnimation(EntityId entityId, HashedString name) const
{
  return m_components.at(entityId)->animations.contains(name);
}

bool SysAnimationImpl::hasAnimationPlaying(EntityId entityId) const
{
  auto i = m_activeAnimations.find(entityId);
  return i != m_activeAnimations.end();
}

} // namespace

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, Logger& logger)
{
  return std::make_unique<SysAnimationImpl>(componentStore, logger);
}

#include "sys_animation.hpp"
#include "sys_render.hpp"
#include <chrono>
#include <map>
#include <cassert>
#include "game_events.hpp"

namespace
{

// TODO: Share animations?
struct AnimationState
{
  HashedString name;
  std::array<AnimationFrame, MAX_ANIMATION_FRAMES> frames;
  uint32_t numFrames;
  Tick duration = 1;
  Tick timeStarted = 0;
  size_t currentFrame = 0;
  Vec2f startPos;
  Vec4f startColour;
  bool isRunning = false;
};

struct CAnimationData
{
  std::array<AnimationState, MAX_ANIMATIONS> animations;
  int currentAnimation = -1;

  static constexpr ComponentType TypeId = ComponentTypeId::CAnimationTypeId;
};

// Uncomment to see difference
//const long diff = sizeof(CAnimationData) - sizeof(CAnimationView);
static_assert(sizeof(CAnimationData) == sizeof(CAnimationView));

class SysAnimationImpl : public SysAnimation
{
  public:
    SysAnimationImpl(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const GameEvent& event) override {}

    void addEntity(EntityId entityId, const CAnimation& data) override;
    void playAnimation(EntityId entityId, HashedString name) override;
    bool hasAnimationPlaying(EntityId entityId) const override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    ComponentStore& m_componentStore;
    std::map<EntityId, std::map<HashedString, size_t>> m_animations;
    Tick m_currentTick = 0;
};

SysAnimationImpl::SysAnimationImpl(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger)
  : m_logger(logger)
  , m_eventSystem(eventSystem)
  , m_componentStore(componentStore)
{
}

void SysAnimationImpl::update(Tick tick)
{
  m_currentTick = tick;

  // TODO: Consider storing list of entities with currently playing animations rather than iterating
  // over all entities.
  for (auto& group : m_componentStore.components<CRenderView, CAnimationData>()) {
    auto renderComps = group.components<CRenderView>();
    auto animComps = group.components<CAnimationData>();
    auto& entityIds = group.entityIds();

    size_t n = group.numEntities();

    for (size_t i = 0; i < n; ++i) {
      auto& renderComp = renderComps[i];
      auto& animComp = animComps[i];

      if (animComp.currentAnimation == -1) {
        continue;
      }

      auto& anim = animComp.animations[animComp.currentAnimation];
      assert(anim.isRunning);

      size_t numFrames = anim.numFrames;
      float frameDuration = static_cast<float>(anim.duration) / (numFrames * TICKS_PER_SECOND);
      float currentFrameTime = static_cast<float>(anim.currentFrame) * frameDuration;
      float nextFrameTime = static_cast<float>(anim.currentFrame + 1) * frameDuration;

      float tickDuration = 1.f / TICKS_PER_SECOND;
      float elapsed = tickDuration * (tick - anim.timeStarted);

      const auto& frame = anim.frames[anim.currentFrame];

      float s = std::min((elapsed - currentFrameTime) / frameDuration, 1.f);
      assert(s >= 0.f);

      renderComp.pos = anim.startPos + frame.delta * (static_cast<float>(anim.currentFrame) + s);

      if (elapsed >= nextFrameTime) {
        renderComp.textureRect = frame.textureRect;
        renderComp.colour = frame.colour;

        ++anim.currentFrame;

        if (anim.currentFrame == numFrames) {
          anim.isRunning = false;
          anim.currentFrame = 0;
          animComp.currentAnimation = -1;

          m_eventSystem.fireEvent(EAnimationFinished{entityIds[i], anim.name, { entityIds[i] }});
        }
      }
    }
  }
}

void SysAnimationImpl::removeEntity(EntityId entityId)
{
  m_animations.erase(entityId);
}

bool SysAnimationImpl::hasEntity(EntityId entityId) const
{
  return m_animations.contains(entityId);
}

void SysAnimationImpl::addEntity(EntityId entityId, const CAnimation& data)
{
  if (data.animations.size() > MAX_ANIMATIONS) {
    EXCEPTION("Entities cannot have more than " << MAX_ANIMATIONS << " animations");
  }

  std::array<AnimationState, MAX_ANIMATIONS> animations;

  auto extractFrames = [](const Animation& a) {
    if (a.frames.size() > MAX_ANIMATION_FRAMES) {
      EXCEPTION("Animations cannot have more than " << MAX_ANIMATION_FRAMES << " frames");
    }

    std::array<AnimationFrame, MAX_ANIMATION_FRAMES> frames;

    for (size_t i = 0; i < a.frames.size(); ++i) {
      frames[i] = a.frames[i];
    }

    return frames;
  };

  size_t i = 0;
  for (auto& anim : data.animations) {
    animations[i] = AnimationState{
      .name = anim.name,
      .frames = extractFrames(anim),
      .numFrames = static_cast<uint32_t>(anim.frames.size()),
      .duration = anim.duration,
      .timeStarted = 0,
      .currentFrame = 0,
      .startPos = Vec2f{},
      .isRunning = false
    };
    m_animations[entityId].insert({ anim.name, i });
    ++i;
  }

  m_componentStore.component<CAnimationData>(entityId) = CAnimationData{
    .animations = animations,
    .currentAnimation = -1
  };
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name)
{
  size_t i = m_animations.at(entityId).at(name);
  auto& animComp = m_componentStore.component<CAnimationData>(entityId);
  auto& renderComp = m_componentStore.component<CRenderView>(entityId);

  auto& anim = animComp.animations[i];
  if (anim.isRunning) {
    return;
  }
  anim.timeStarted = m_currentTick;
  anim.isRunning = true;
  anim.currentFrame = 0;
  anim.startPos = renderComp.pos;
  animComp.currentAnimation = i;
}

bool SysAnimationImpl::hasAnimationPlaying(EntityId entityId) const
{
  return m_componentStore.component<CAnimationData>(entityId).currentAnimation != -1;
}

} // namespace

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger)
{
  return std::make_unique<SysAnimationImpl>(componentStore, eventSystem, logger);
}

#include "sys_animation.hpp"
#include "sys_render.hpp"
#include <chrono>
#include <map>
#include <cassert>

namespace
{

struct AnimationState
{
  Animation animation;  // TODO: Share animations?
  Tick timeStarted = 0;
  bool isRunning = false;
  size_t currentFrame = 0;
};

constexpr size_t MAX_ANIMATIONS = 8;

struct CAnimationData
{
  std::array<AnimationState, MAX_ANIMATIONS> animations;
  int currentAnimation = -1;

  static constexpr ComponentType TypeId = ComponentTypeId::CAnimationTypeId;
};

// Uncomment to find difference
//const size_t diff = sizeof(CAnimationData) - sizeof(CAnimationView);
static_assert(sizeof(CAnimationData) == sizeof(CAnimationView));

class SysAnimationImpl : public SysAnimation
{
  public:
    SysAnimationImpl(ComponentStore& componentStore, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const GameEvent& event) override {}

    void addEntity(EntityId entityId, const CAnimation& data) override;
    void playAnimation(EntityId entityId, HashedString name) override;
    bool hasAnimationPlaying(EntityId entityId) const override;

  private:
    Logger& m_logger;
    ComponentStore& m_componentStore;
    std::map<EntityId, std::map<HashedString, size_t>> m_animations;
    Tick m_currentTick = 0;
};

SysAnimationImpl::SysAnimationImpl(ComponentStore& componentStore, Logger& logger)
  : m_logger(logger)
  , m_componentStore(componentStore)
{
}

void SysAnimationImpl::update(Tick tick)
{
  m_currentTick = tick;

  for (auto& group : m_componentStore.components<CRenderView, CAnimationData>()) {
    auto renderComps = group.components<CRenderView>();
    auto animComps = group.components<CAnimationData>();
    size_t n = group.numEntities();

    for (size_t i = 0; i < n; ++i) {
      auto& renderComp = renderComps[i];
      auto& animComp = animComps[i];

      if (animComp.currentAnimation == -1) {
        continue;
      }

      auto& anim = animComp.animations[animComp.currentAnimation];
      assert(anim.isRunning);

      size_t numFrames = anim.animation.frames.size();
      float frameDuration = static_cast<float>(anim.animation.duration)
        / (numFrames * TICKS_PER_SECOND);
      float lastFrame = anim.currentFrame * frameDuration;

      float tickDuration = 1.f / TICKS_PER_SECOND;
      float elapsed = (tick - anim.timeStarted) * tickDuration;

      if (tick == anim.timeStarted || elapsed > lastFrame) {
        const auto& frame = anim.animation.frames[anim.currentFrame];

        renderComp.pos += frame.delta;
        // TODO: Update texture rect

        anim.currentFrame = (anim.currentFrame + 1) % numFrames;

        if (anim.currentFrame == 0) {
          anim.isRunning = false;
          anim.currentFrame = 0;
          animComp.currentAnimation = -1;
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
  std::array<AnimationState, MAX_ANIMATIONS> animations;

  size_t i = 0;
  for (auto& anim : data.animations) {
    animations[i] = AnimationState{
      .animation = anim,
      .timeStarted = 0,
      .isRunning = false,
      .currentFrame = 0
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
  auto& component = m_componentStore.component<CAnimationData>(entityId);
  auto& anim = component.animations[i];
  if (anim.isRunning) {
    return;
  }
  anim.timeStarted = m_currentTick;
  anim.isRunning = true;
  anim.currentFrame = 0;
  component.currentAnimation = i;
}

bool SysAnimationImpl::hasAnimationPlaying(EntityId entityId) const
{
  return m_componentStore.component<CAnimationData>(entityId).currentAnimation != -1;
}

} // namespace

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, Logger& logger)
{
  return std::make_unique<SysAnimationImpl>(componentStore, logger);
}

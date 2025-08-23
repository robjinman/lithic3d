#include "sys_animation.hpp"
#include "sys_render.hpp"
#include "game_events.hpp"
#include <chrono>
#include <map>
#include <cassert>

namespace
{

struct AnimationState
{
  AnimationId id;
  HashedString name;
  bool isPlaying = false;
  Tick timeStarted = 0;
  size_t currentFrame = 0;
  Vec2f startPos;
  Vec4f startColour;
  bool shouldRepeat = false;
};

struct CAnimationData
{
  std::map<HashedString, AnimationId> animations;
};

using CAnimationDataPtr = std::unique_ptr<CAnimationData>;

class SysAnimationImpl : public SysAnimation
{
  public:
    SysAnimationImpl(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick) override;
    void processEvent(const GameEvent& event) override {}

    void addEntity(EntityId entityId, const CAnimation& data) override;
    AnimationId addAnimation(AnimationPtr animation) override;
    void playAnimation(EntityId entityId, HashedString name, bool repeat) override;
    bool hasAnimationPlaying(EntityId entityId) const override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    ComponentStore& m_componentStore;
    std::map<EntityId, CAnimationDataPtr> m_components;
    std::map<EntityId, AnimationState> m_activeAnimation;
    std::map<AnimationId, AnimationPtr> m_animations;
    Tick m_currentTick = 0;

    static AnimationId m_nextId;
};

AnimationId SysAnimationImpl::m_nextId = 0;

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

  std::vector<std::pair<EntityId, HashedString>> toRepeat;

  for (auto i = m_activeAnimation.begin(); i != m_activeAnimation.end(); ++i) {
    auto entityId = i->first;
    auto& animState = i->second;

    if (!animState.isPlaying) {
      continue;
    }

    auto& anim = *m_animations.at(animState.id);
    auto& renderComp = m_componentStore.component<CRenderView>(entityId);

    size_t numFrames = anim.frames.size();
    float frameDuration = static_cast<float>(anim.duration) / (numFrames * TICKS_PER_SECOND);
    float currentFrameTime = static_cast<float>(animState.currentFrame) * frameDuration;
    float nextFrameTime = static_cast<float>(animState.currentFrame + 1) * frameDuration;

    float tickDuration = 1.f / TICKS_PER_SECOND;
    float elapsed = tickDuration * (tick - animState.timeStarted);

    const auto& frame = anim.frames[animState.currentFrame];

    float s = std::min((elapsed - currentFrameTime) / frameDuration, 1.f);
    assert(s >= 0.f);

    renderComp.pos = animState.startPos +
      frame.delta * (static_cast<float>(animState.currentFrame) + s);

    if (elapsed >= nextFrameTime) {
      if (frame.textureRect.has_value()) {
        renderComp.textureRect = frame.textureRect.value();
      }
      renderComp.colour = frame.colour;

      ++animState.currentFrame;

      if (animState.currentFrame == numFrames) {
        animState.isPlaying = false;

        auto e = std::make_unique<EAnimationFinished>(entityId, anim.name, EntityIdSet{ entityId });
        m_eventSystem.queueEvent(std::move(e));

        if (animState.shouldRepeat) {
          renderComp.pos = animState.startPos;
          toRepeat.push_back({ entityId, anim.name });
        }

        i = m_activeAnimation.erase(i);
        if (i == m_activeAnimation.end()) {
          break;
        }
      }
    }
  }

  for (auto& anim : toRepeat) {
    playAnimation(anim.first, anim.second, true);
  }
}

void SysAnimationImpl::addEntity(EntityId entityId, const CAnimation& comp)
{
  auto data = std::make_unique<CAnimationData>();

  for (auto animId : comp.animations) {
    auto& anim = *m_animations.at(animId);
    data->animations.insert({ anim.name, animId });
  }

  m_components.insert({ entityId, std::move(data) });
}

void SysAnimationImpl::removeEntity(EntityId entityId)
{
  m_activeAnimation.erase(entityId);
  m_components.erase(entityId);
}

bool SysAnimationImpl::hasEntity(EntityId entityId) const
{
  return m_components.contains(entityId);
}

AnimationId SysAnimationImpl::addAnimation(AnimationPtr animation)
{
  auto id = m_nextId++;

  m_animations.insert({ id, std::move(animation) });

  return id;
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name, bool repeat)
{
  auto& renderComp = m_componentStore.component<CRenderView>(entityId);
  auto& animComp = *m_components.at(entityId);
  auto animId = animComp.animations.at(name);

  m_activeAnimation.insert({ entityId, AnimationState{
    .id = animId,
    .name = name,
    .isPlaying = true,
    .timeStarted = m_currentTick,
    .currentFrame = 0,
    .startPos = renderComp.pos,
    .startColour = renderComp.colour,
    .shouldRepeat = repeat
  }});
}

bool SysAnimationImpl::hasAnimationPlaying(EntityId entityId) const
{
  auto i = m_activeAnimation.find(entityId);
  return i != m_activeAnimation.end() && i->second.isPlaying;
}

} // namespace

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger)
{
  return std::make_unique<SysAnimationImpl>(componentStore, eventSystem, logger);
}

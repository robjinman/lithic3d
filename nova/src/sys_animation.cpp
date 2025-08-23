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
    void seek(EntityId entityId, Tick tick) override;
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
    Tick elapsed = tick - animState.timeStarted;
    float_t fractionComplete = static_cast<float_t>(elapsed) / anim.duration;
    float_t frameNumFloat = fractionComplete * numFrames;
    size_t frameNumInt = static_cast<size_t>(frameNumFloat);
    float_t fractionOfFrameComplete = frameNumFloat - frameNumInt;

    assert(numFrames > 0);
    assert(frameNumInt <= numFrames);

    Vec2f delta;
    for (size_t f = 0; f < frameNumInt; ++f) {
      delta += anim.frames[f].delta;
    }
    delta += anim.frames[frameNumInt].delta * fractionOfFrameComplete;
    renderComp.pos = animState.startPos + delta;

    if (frameNumInt == numFrames) {
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
    else {
      auto& frame = anim.frames[frameNumInt];

      if (frame.textureRect.has_value()) {
        renderComp.textureRect = frame.textureRect.value();
      }
      if (frame.colour.has_value()) {
        renderComp.colour = frame.colour.value();
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
    .startPos = renderComp.pos,
    .startColour = renderComp.colour,
    .shouldRepeat = repeat
  }});
}

void SysAnimationImpl::seek(EntityId entityId, Tick tick)
{
  auto i = m_activeAnimation.find(entityId);
  if (i == m_activeAnimation.end()) {
    EXCEPTION("Entity doesn't have animation playing");
  }

  auto& state = i->second;
  state.timeStarted = m_currentTick - tick;
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

#include "sys_animation.hpp"
#include "sys_render.hpp"
#include "sys_spatial.hpp"
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
  Tick tick = 0;
  Mat4x4f initialT;
  Vec4f startColour;
  bool repeats = false;
};

struct CAnimation
{
  std::map<HashedString, AnimationId> animations;
};

using CAnimationPtr = std::unique_ptr<CAnimation>;

class SysAnimationImpl : public SysAnimation
{
  public:
    SysAnimationImpl(ComponentStore& componentStore, EventSystem& eventSystem, Logger& logger);

    void removeEntity(EntityId entityId) override;
    bool hasEntity(EntityId entityId) const override;
    void update(Tick tick, const InputState& inputState) override;
    void processEvent(const Event& event) override {}

    void addEntity(EntityId entityId, const AnimationData& data) override;
    AnimationId addAnimation(AnimationPtr animation) override;
    void playAnimation(EntityId entityId, HashedString name, bool repeat) override;
    void seek(EntityId entityId, Tick tick) override;
    bool hasAnimationPlaying(EntityId entityId) const override;

  private:
    Logger& m_logger;
    EventSystem& m_eventSystem;
    ComponentStore& m_componentStore;
    std::map<EntityId, CAnimationPtr> m_components;
    std::map<EntityId, AnimationState> m_activeAnimations;
    std::map<AnimationId, AnimationPtr> m_animations;

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

void SysAnimationImpl::update(Tick, const InputState&)
{
  // Entity id, animation name, bring to front
  std::vector<std::tuple<EntityId, HashedString>> toRepeat;

  for (auto i = m_activeAnimations.begin(); i != m_activeAnimations.end();) {
    auto entityId = i->first;
    auto& animState = i->second;

    assert(animState.isPlaying);

    auto& flags = m_componentStore.component<CSpatialFlags>(entityId);
    if (!(flags.enabled && flags.parentEnabled)) {
      ++i;
      continue;
    }

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

      localTComp.transform = translationMatrix4x4(pos) * animState.initialT * scaleMatrix4x4(scale);

      animState.isPlaying = false;

      auto e = std::make_unique<EAnimationFinish>(entityId, anim.name, EntityIdSet{ entityId });
      m_eventSystem.queueEvent(std::move(e));

      if (animState.repeats) {
        localTComp.transform = animState.initialT;
        toRepeat.push_back({ entityId, anim.name });
      }

      i = m_activeAnimations.erase(i);
    }
    else {
      float_t fractionOfFrameComplete = frameNumFloat - frameNumInt;
      auto& frame = anim.frames[frameNumInt];

      Vec2f prevPos = frameNumInt > 0 ? anim.frames[frameNumInt - 1].pos : Vec2f{};
      Vec2f delta = frame.pos - prevPos;
      Vec2f interp = prevPos + delta * fractionOfFrameComplete;

      Vec3f pos{ interp[0], interp[1], 0.f };
      Vec3f scale{ frame.scale[0], frame.scale[1], 1.f };

      localTComp.transform = translationMatrix4x4(pos) * animState.initialT * scaleMatrix4x4(scale);

      if (frame.textureRect.has_value()) {
        renderComp.textureRect = frame.textureRect.value();
      }
      if (frame.colour.has_value()) {
        // TODO: interpolate
        renderComp.colour = frame.colour.value();
      }

      ++i;
    }
  }

  for (auto& anim : toRepeat) {
    playAnimation(std::get<0>(anim), std::get<1>(anim), true);
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

  m_animations.insert({ id, std::move(animation) });

  return id;
}

void SysAnimationImpl::playAnimation(EntityId entityId, HashedString name, bool repeat)
{
  auto& renderComp = m_componentStore.component<CRender>(entityId);
  auto& localTComp = m_componentStore.component<CLocalTransform>(entityId);
  auto& animComp = *m_components.at(entityId);
  auto animId = animComp.animations.at(name);

  m_activeAnimations.insert({ entityId, AnimationState{
    .id = animId,
    .name = name,
    .isPlaying = true,
    .tick = 0,
    .initialT = localTComp.transform,
    .startColour = renderComp.colour,
    .repeats = repeat
  }});
}

void SysAnimationImpl::seek(EntityId entityId, Tick tick)
{
  auto i = m_activeAnimations.find(entityId);
  if (i == m_activeAnimations.end()) {
    EXCEPTION("Entity doesn't have animation playing");
  }

  auto& state = i->second;
  state.tick = tick;
}

bool SysAnimationImpl::hasAnimationPlaying(EntityId entityId) const
{
  auto i = m_activeAnimations.find(entityId);
  return i != m_activeAnimations.end() && i->second.isPlaying;
}

} // namespace

SysAnimationPtr createSysAnimation(ComponentStore& componentStore, EventSystem& eventSystem,
  Logger& logger)
{
  return std::make_unique<SysAnimationImpl>(componentStore, eventSystem, logger);
}

#include "b_animation.hpp"
#include "sys_render.hpp"
#include <chrono>
#include <map>
#include <cassert>

namespace
{

struct AnimationState
{
  Animation animation;
  Tick timeStarted = 0;
  bool isRunning = false;
  size_t currentFrame = 0;
};

class BAnimationImpl : public BAnimation
{
  public:
    BAnimationImpl(EntityId entityId, SysRender& sysRender);

    HashedString name() const override;
    const std::set<HashedString>& subscriptions() const override;
    void processEvent(const GameEvent&) override {}
    void update(Tick tick, const InputState& inputState) override;

    void addAnimation(const Animation& animation) override;
    void playAnimation(HashedString name) override;
    bool hasAnimationPlaying() const override;

  private:
    EntityId m_entityId;
    SysRender& m_sysRender;
    std::map<HashedString, AnimationState> m_animations;
    Tick m_currentTick = 0;
    HashedString m_currentAnimation = 0;
};

BAnimationImpl::BAnimationImpl(EntityId entityId, SysRender& sysRender)
  : m_entityId(entityId),
    m_sysRender(sysRender)
{
}

HashedString BAnimationImpl::name() const
{
  static auto strAnimation = hashString("animation");
  return strAnimation;
}

const std::set<HashedString>& BAnimationImpl::subscriptions() const
{
  static std::set<HashedString> subs{};
  return subs;
}

void BAnimationImpl::update(Tick tick, const InputState&)
{
  m_currentTick = tick;

  if (m_currentAnimation == 0) {
    return;
  }

  auto& anim = m_animations.at(m_currentAnimation);
  assert(anim.isRunning);

  size_t numFrames = anim.animation.frames.size();
  float frameDuration = static_cast<float>(anim.animation.duration)
    / (numFrames * TICKS_PER_SECOND);
  float lastFrame = anim.currentFrame * frameDuration;

  float tickDuration = 1.f / TICKS_PER_SECOND;
  float elapsed = (tick - anim.timeStarted) * tickDuration;

  if (tick == anim.timeStarted || elapsed > lastFrame) {
    const auto& frame = anim.animation.frames[anim.currentFrame];

    m_sysRender.move(m_entityId, frame.delta);
    m_sysRender.setTextureRect(m_entityId, frame.textureRect);

    anim.currentFrame = (anim.currentFrame + 1) % numFrames;

    if (anim.currentFrame == 0) {
      anim.isRunning = false;
      m_currentAnimation = 0;
    }
  }
}

void BAnimationImpl::addAnimation(const Animation& animation)
{
  m_animations[animation.name] = AnimationState{
    .animation = animation,
    .timeStarted = 0,
    .isRunning = false
  };
}

void BAnimationImpl::playAnimation(HashedString name)
{
  auto& anim = m_animations.at(name);

  if (anim.isRunning) {
    return;
  }

  anim.timeStarted = m_currentTick;
  anim.isRunning = true;
  m_currentAnimation = name;
}

bool BAnimationImpl::hasAnimationPlaying() const
{
  return m_currentAnimation != 0;
}

}

BAnimationPtr createBAnimation(EntityId entityId, SysRender& sysRender)
{
  return std::make_unique<BAnimationImpl>(entityId, sysRender);
}

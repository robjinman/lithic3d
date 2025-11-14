#include "player.hpp"
#include <lithic3d/lithic3d.hpp>

using namespace lithic3d;

namespace
{

class PlayerImpl : public Player
{
  public:
    PlayerImpl(Camera3d& camera);

    const Vec3f& getPosition() const override;
    void setPosition(const Vec3f& position) override;
    Vec3f getDirection() const override;
    virtual void translate(const Vec3f& delta) override;
    virtual void rotate(float deltaYaw, float deltaPitch) override;
    float getStepHeight() const override;
    float getSpeed() const override;
    float getRadius() const override;
    float getJumpHeight() const override;

  private:
    Camera3d& m_camera;
    Vec3f m_position;
    float m_speed = metresToWorldUnits(9.f);
    float m_radius = metresToWorldUnits(0.4f);
    float m_tallness = metresToWorldUnits(1.7f);
    float m_stepHeight = metresToWorldUnits(0.3f);
    float m_bounceHeight = metresToWorldUnits(0.035f);
    float m_bounceRate = 3.5f;
    float m_jumpHeight = metresToWorldUnits(0.8f);

    float m_originalTallness;
    float m_distance = 0;
};

PlayerImpl::PlayerImpl(Camera3d& camera)
  : m_camera(camera)
{
  m_originalTallness = m_tallness;
  translate({});
}

const Vec3f& PlayerImpl::getPosition() const
{
  return m_position;
}

Vec3f PlayerImpl::getDirection() const
{
  return m_camera.getDirection();
}

void PlayerImpl::translate(const Vec3f& delta)
{
  const float dx = m_bounceRate * 2.f * PIf / TICKS_PER_SECOND;

  if (delta[0] != 0 || delta[2] != 0) {
    m_tallness = m_originalTallness + m_bounceHeight * sin(m_distance);
    m_distance += dx;
  }

  m_position += delta;
  m_camera.setPosition(m_position + Vec3f{ 0, m_tallness, 0 });
}

void PlayerImpl::rotate(float deltaPitch, float deltaYaw)
{
  m_camera.rotate(deltaPitch, deltaYaw);
}

void PlayerImpl::setPosition(const Vec3f& position)
{
  m_position = position;
  m_camera.setPosition(m_position + Vec3f{ 0, m_tallness, 0 });
}

float PlayerImpl::getStepHeight() const
{
  return m_stepHeight;
}

float PlayerImpl::getSpeed() const
{
  return m_speed;
}

float PlayerImpl::getRadius() const
{
  return m_radius;
}

float PlayerImpl::getJumpHeight() const
{
  return m_jumpHeight;
}

} // namespace

PlayerPtr createPlayer(Camera3d& camera)
{
  return std::make_unique<PlayerImpl>(camera);
}

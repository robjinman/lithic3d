#version 450
#extension GL_ARB_separate_shader_objects : enable

const float RANDOM[] = {
  0.9353, 0.0035, 0.4084, 0.0346, 0.3617, 0.2028, 0.1593, 0.0854, 0.6967, 0.7926,
  0.6372, 0.3049, 0.3912, 0.4105, 0.3040, 0.8758, 0.7543, 0.1433, 0.3685, 0.3557,
  0.4649, 0.2566, 0.0142, 0.1702, 0.9150, 0.9397, 0.9525, 0.7027, 0.7242, 0.3794,
  0.8918, 0.0186, 0.0660, 0.9046, 0.4676, 0.8259, 0.4862, 0.7008, 0.7787, 0.8437,
  0.4434, 0.7409, 0.5025, 0.0512, 0.2225, 0.7767, 0.4757, 0.4960, 0.6877, 0.4581,
  0.8148, 0.2011, 0.9221, 0.2488, 0.3596, 0.2979, 0.3746, 0.2518, 0.7016, 0.6160,
  0.3914, 0.7184, 0.1455, 0.2432, 0.9424, 0.5158, 0.3793, 0.8635, 0.4986, 0.8496,
  0.9986, 0.5086, 0.6134, 0.1722, 0.3444, 0.2514, 0.8790, 0.0646, 0.8365, 0.3125,
  0.2859, 0.9785, 0.0003, 0.3276, 0.4918, 0.8275, 0.4329, 0.6160, 0.6184, 0.0937,
  0.0595, 0.0956, 0.2397, 0.2999, 0.9818, 0.1288, 0.0071, 0.7785, 0.0496, 0.7227
};

struct Particle
{
  vec3 position;
  vec4 colour;
  vec3 velocity;
};

layout(push_constant) uniform PushConstants
{
  int tick;
} constants;

//layout(std140, set = 0, binding = 0) readonly buffer ParticlesUbo
//{
//} emitter;

layout(std140, set = 0, binding = 0) readonly buffer ParticlesInSsbo
{
  Particle particlesIn[];
};

layout(std140, set = 0, binding = 1) writeonly buffer ParticlesOutSsbo
{
  Particle particlesOut[];
};

layout (local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

void main()
{
  const uint index = gl_GlobalInvocationID.x;
  Particle particleIn = particlesIn[index];

  particlesOut[index].position = particleIn.position + particleIn.velocity;
  particleIn.colour.a *= 1.0 / (1.0 + particleIn.velocity.y);
  particlesOut[index].colour = particleIn.colour;
  particlesOut[index].velocity.y = particleIn.velocity.y + RANDOM[index % 100] * 0.01;

  if (particleIn.colour.a < 0.005) {
    particlesOut[index].velocity.y = 0.1;
    particlesOut[index].position.y = 0.0;
    particlesOut[index].colour.a = 0.8;
  }
}

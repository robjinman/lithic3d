#version 450
#extension GL_ARB_separate_shader_objects : enable

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
/*
layout(std140, set = 0, binding = 0) readonly buffer ParticlesUbo
{
  // TODO
  int dummy;
};*/

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

  int tick = constants.tick % 60;

  if (tick == 0) {
    particlesOut[index].position = vec3(0, 0, 0);
  }
  else {
    float t = float(tick) / 60.0;
    float s = 0.5;
    particlesOut[index].position = particleIn.position + particleIn.velocity * s * t;
  }

  particlesOut[index].colour = particleIn.colour;
  particlesOut[index].velocity = particleIn.velocity;
}

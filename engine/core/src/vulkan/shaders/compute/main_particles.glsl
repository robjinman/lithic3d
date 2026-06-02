#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Particle
{
  vec3 position;
  vec3 velocity;
  vec4 color;
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

  if (constants.tick % 30 == 0) {
    particlesOut[index].position = vec3(0, 0, 0);
    particlesOut[index].velocity = particleIn.velocity;
  }
  else {
    float t = float(constants.tick) / 60.0;
    particlesOut[index].position = particleIn.position + particleIn.velocity * t;
    particlesOut[index].velocity = particleIn.velocity;
  }
}

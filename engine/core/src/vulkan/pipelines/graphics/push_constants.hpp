#pragma once

#include "lithic3d/math.hpp"

namespace lithic3d
{
namespace render
{

// Remember to update offsets in shader after updating these
#pragma pack(push, 4)
struct DefaultPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
  uint32_t shadowCascade;     // 4 bytes
  char _padding0[12];         // 12 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
  uint32_t tick;              // 4 bytes
};

constexpr size_t DEFAULT_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t DEFAULT_PUSH_CONSTANTS_VERT_SIZE = 68;
constexpr size_t DEFAULT_PUSH_CONSTANTS_FRAG_OFFSET = 80;
constexpr size_t DEFAULT_PUSH_CONSTANTS_FRAG_SIZE = 20;

struct QuadPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
  float radius;               // 4 bytes
};

constexpr size_t QUAD_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t QUAD_PUSH_CONSTANTS_VERT_SIZE = 64;
constexpr size_t QUAD_PUSH_CONSTANTS_FRAG_OFFSET = 64;
constexpr size_t QUAD_PUSH_CONSTANTS_FRAG_SIZE = 20;

struct SpritePushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
  Vec2f spriteUvCoords[4];    // 32 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};

constexpr size_t SPRITE_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t SPRITE_PUSH_CONSTANTS_VERT_SIZE = 96;
constexpr size_t SPRITE_PUSH_CONSTANTS_FRAG_OFFSET = 96;
constexpr size_t SPRITE_PUSH_CONSTANTS_FRAG_SIZE = 16;

struct DynamicTextPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
  uint32_t text[8];           // 32 bytes

  // Frag shader
  Vec4f colour;               // 16 bytes
};

constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_VERT_SIZE = 96;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_OFFSET = 96;
constexpr size_t DYNAMIC_TEXT_PUSH_CONSTANTS_FRAG_SIZE = 16;

struct ParticlesPushConstants
{
  // Vert shader
  Mat4x4f modelMatrix;        // 64 bytes
};

constexpr size_t PARTICLES_PUSH_CONSTANTS_VERT_OFFSET = 0;
constexpr size_t PARTICLES_PUSH_CONSTANTS_VERT_SIZE = 64;
#pragma pack(pop)

} // namespace render
} // namespace lithic3d

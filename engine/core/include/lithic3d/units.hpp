#pragma once

#include "math.hpp"

// TODO: Remove Minefield specific shit

namespace lithic3d
{

using Tick = long;

const size_t TICKS_PER_SECOND = 60;
const float WORLD_UNITS_PER_METRE = 10;

inline float metresToWorldUnits(float x)
{
  return x * WORLD_UNITS_PER_METRE;
}

template<size_t N>
Vector<float, N> metresToWorldUnits(const Vector<float, N>& v)
{
  Vector<float, N> k;
  for (size_t i = 0; i < N; ++i) {
    k[i] = metresToWorldUnits(v[i]);
  }
  return k;
}

using ScissorId = size_t;

inline float pxToUvX(float px, float textureWidthPx)
{
  px += 0.5f;
  return px / textureWidthPx;
}

inline float pxToUvY(float pxY, float pxH, float textureHeightPx)
{
  pxY += 0.5f;
  pxH -= 1.f;
  return (textureHeightPx - pxY - pxH) / textureHeightPx;
}

inline float pxToUvW(float pw, float textureWidthPx)
{
  pw -= 1.f;
  return pw / textureWidthPx;
}

inline float pxToUvH(float ph, float textureHeightPx)
{
  ph -= 1.f;
  return ph / textureHeightPx;
}

} // namespace lithic3d

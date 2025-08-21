#pragma once

#include "math.hpp"

using Tick = long;

const size_t TICKS_PER_SECOND = 60;
const float_t WORLD_UNITS_PER_METRE = 10;

inline float_t metresToWorldUnits(float_t x)
{
  return x * WORLD_UNITS_PER_METRE;
}

template<size_t N>
Vector<float_t, N> metresToWorldUnits(const Vector<float_t, N>& v)
{
  Vector<float_t, N> k;
  for (size_t i = 0; i < N; ++i) {
    k[i] = metresToWorldUnits(v[i]);
  }
  return k;
}

const uint32_t ATLAS_WIDTH_PX = 1024;
const uint32_t ATLAS_HEIGHT_PX = 512;

inline float_t pxToUvX(float_t px)
{
  return px / static_cast<float_t>(ATLAS_WIDTH_PX);
}

inline float_t pxToUvY(float_t pxY, float_t pxH)
{
  return (static_cast<float_t>(ATLAS_HEIGHT_PX) - pxY - pxH) / static_cast<float_t>(ATLAS_HEIGHT_PX);
}

inline float_t pxToUvW(float_t pw)
{
  return pw / static_cast<float_t>(ATLAS_WIDTH_PX);
}

inline float_t pxToUvH(float_t ph)
{
  return ph / static_cast<float_t>(ATLAS_HEIGHT_PX);
}

const int GRID_W = 21;
const int GRID_H = 11;

const float_t GRID_CELL_W = 0.0625;
const float_t GRID_CELL_H = 0.0625;

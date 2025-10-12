#pragma once

#include "math.hpp"

using Tick = long;

const size_t TICKS_PER_SECOND = 60;
const float WORLD_UNITS_PER_METRE = 10;

const size_t MAIN_VIEWPORT = 1;
const size_t MOBILE_CONTROLS_VIEWPORT = 2;

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

const uint32_t ATLAS_WIDTH_PX = 1024;
const uint32_t ATLAS_HEIGHT_PX = 512;

inline float pxToUvX(float px)
{
  px += 0.5f;
  return px / static_cast<float>(ATLAS_WIDTH_PX);
}

inline float pxToUvY(float pxY, float pxH)
{
  pxY += 0.5f;
  pxH -= 1.f;
  return (static_cast<float>(ATLAS_HEIGHT_PX) - pxY - pxH) /
    static_cast<float>(ATLAS_HEIGHT_PX);
}

inline float pxToUvW(float pw)
{
  pw -= 1.f;
  return pw / static_cast<float>(ATLAS_WIDTH_PX);
}

inline float pxToUvH(float ph)
{
  ph -= 1.f;
  return ph / static_cast<float>(ATLAS_HEIGHT_PX);
}

const int GRID_W = 21;
const int GRID_H = 11;

const float GRID_CELL_W = 0.0625;
const float GRID_CELL_H = 0.0625;

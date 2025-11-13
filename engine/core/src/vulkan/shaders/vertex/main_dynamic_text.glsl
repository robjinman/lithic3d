#version 450

#include "common.glsl"
#include "vertex/attributes.glsl"

layout(std140, set = DESCRIPTOR_SET_RENDER_PASS, binding = 0) uniform CameraTransformsUbo
{
  mat4 viewMatrix;
  mat4 projMatrix;
} camera;

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
  uint text[8];       // 32 byte string
} constants;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec3 outNormal;

// TODO: Remove
const uint ATLAS_WIDTH_PX = 1024;
const uint ATLAS_HEIGHT_PX = 512;

float pxToUvX(float px)
{
  px += 0.5;
  return px / float(ATLAS_WIDTH_PX);
}

float pxToUvY(float pxY, float pxH)
{
  pxY += 0.5;
  pxH -= 1.0;
  return (float(ATLAS_HEIGHT_PX) - pxY - pxH) / float(ATLAS_HEIGHT_PX);
}

float pxToUvW(float pw)
{
  pw -= 1.0;
  return pw / float(ATLAS_WIDTH_PX);
}

float pxToUvH(float ph)
{
  ph -= 1.0;
  return ph / float(ATLAS_HEIGHT_PX);
}

uint nthChar(uint n)
{
  uint element = n / 4;
  uint offset = n % 4;
  // Assume little endian
  uint shift = offset * 8;
  return (constants.text[element] >> shift) & 0xFF;
}

vec2 uvForVertex(int vertexId)
{
  // TODO: Don't hard code. Use specialisation constants
  const float uvRectX = pxToUvX(256.0);
  const float uvRectY = pxToUvY(64.0, 192.0);
  const float uvRectW = pxToUvW(192.0);
  const float uvRectH = pxToUvH(192.0);
  const uint glyphsPerRow = 12;
  const uint firstGlyph = 32;   // Space
  const uint lastGlyph = 126;   // Tilde
  const uint numGlyphs = lastGlyph - firstGlyph + 1;
  const uint numRows = uint(float(numGlyphs) / glyphsPerRow + 0.5);

  const float glyphW = uvRectW / glyphsPerRow;
  const float glyphH = uvRectH / numRows;

  uint n = vertexId / 4;
  uint codePoint = nthChar(n);

  if (codePoint == 0) {
    codePoint = 32; // Space
  }

  // 0: Bottom-left
  // 1: Bottom-right
  // 2: Top-right
  // 3: Top-left
  uint vertex = vertexId % 4;

  uint c = codePoint - firstGlyph;
  uint row = c / glyphsPerRow;
  uint col = c % glyphsPerRow;
  float x0 = uvRectX + glyphW * col;
  float x1 = x0 + glyphW;
  float y0 = uvRectY + glyphH * row;
  float y1 = y0 + glyphH;

  if (vertex == 0) {
    return vec2(x0, y1);
  }
  else if (vertex == 1) {
    return vec2(x1, y1);
  }
  else if (vertex == 2) {
    return vec2(x1, y0);
  }
  else if (vertex == 3) {
    return vec2(x0, y0);
  }
}

vec4 computeVertexPosition(mat4 modelMatrix)
{
  return modelMatrix * vec4(inPos, 1.0);
}

void main()
{
  mat4 modelMatrix = constants.modelMatrix;

  vec4 worldPos = computeVertexPosition(modelMatrix);
  gl_Position = camera.projMatrix * camera.viewMatrix * worldPos;

  outWorldPos = worldPos.xyz;
  outTexCoord = uvForVertex(gl_VertexIndex);
  outNormal = mat3(modelMatrix) * inNormal;
}

#include "lithic3d/math.hpp"
#include "lithic3d/utils.hpp"
#include <cassert>
#include <numeric>

namespace lithic3d
{
namespace
{

Mat2x2f adjoint(const Mat2x2f& M)
{
  return {
    M.at(1, 1), -M.at(0, 1),
    -M.at(1, 0), M.at(0, 0)
  };
}

float determinant(const Mat2x2f& M)
{
  return M.at(0, 0) * M.at(1, 1) - M.at(0, 1) * M.at(1, 0);
}

} // namespace

Line::Line(const Vec2f& A, const Vec2f& B)
{
  a = B[1] - A[1];
  b = A[0] - B[0];
  c = B[0] * A[1] - A[0] * B[1];
}

Line::Line(float a, float b, float c)
  : a(a)
  , b(b)
  , c(c)
{
}

bool lineIntersect(const Line& l1, const Line& l2, Vec2f& p)
{
  if (l1.a * l2.b - l1.b * l2.a != 0) {
    p = {
      (l1.b * l2.c - l1.c * l2.b) / (l1.a * l2.b - l1.b * l2.a),
      (l1.c * l2.a - l1.a * l2.c) / (l1.a * l2.b - l1.b * l2.a)
    };
    return true;
  }
  return false;
}

Vec2f projectionOntoLine(const Line& line, const Vec2f& p)
{
  float a = -line.b;
  float b = line.a;
  float c = -p[1] * b - p[0] * a;
  Line perpendicular(a, b, c);

  Vec2f result;
  [[maybe_unused]] bool intersect = lineIntersect(line, perpendicular, result);
  assert(intersect);

  return result;
}

bool lineSegmentCircleIntersect(const LineSegment& lseg, const Vec2f& centre, float radius)
{
  Vec2f D = lseg.B - lseg.A;
  float alpha = D[0] * D[0] + D[1] * D[1];
  float beta = 2.f * (D[0] * (lseg.A[0] - centre[0]) + D[1] * (lseg.A[1] - centre[1]));
  float gamma = square(lseg.A[0] - centre[0]) + square(lseg.A[1] - centre[1]) - radius * radius;

  float discriminant = beta * beta - 4.f * alpha * gamma;
  if (discriminant < 0) {
    return false;
  }

  float t = (-beta + sqrt(discriminant)) / (2.f * alpha);
  if (t >= 0 && t <= 1.f) {
    return true;
  }

  t = (-beta - sqrt(discriminant)) / (2.f * alpha);
  if (t >= 0 && t <= 1.f) {
    return true;
  }

  return false;
}

bool pointIsInsidePoly(const Vec2f& p, const std::vector<Vec2f>& poly)
{
  bool inside = false;
  int n = static_cast<int>(poly.size());

  for (int i = 0; i < n; ++i) {
    float x1 = poly[i][0];
    float y1 = poly[i][1];

    float x2 = poly[(i + 1) % n][0];
    float y2 = poly[(i + 1) % n][1];

    bool intersects = ((y1 > p[1]) != (y2 > p[1]));
    if (intersects) {
      float xIntersect = x1 + (p[1] - y1) * (x2 - x1) / (y2 - y1);

      if (xIntersect > p[0]) {
        inside = !inside;
      }
    }
  }

  return inside;
}

std::vector<uint16_t> triangulatePoly(const std::vector<Vec3f>& vertices)
{
  ASSERT(vertices.size() >= 3, "Cannot triangulate polygon with < 3 vertices");
  std::vector<uint16_t> indices;

  size_t h = 2; // Index of z component

  auto anticlockwise = [h](const Vec3f& A, const Vec3f& B, const Vec3f& C) {
    return A[0] * B[h] - A[h] * B[0] + A[h] * C[0] - A[0] * C[h] + B[0] * C[h] - C[0] * B[h] > 0.f;
  };

  auto pointInTriangle = [h](const Vec3f& P, const Vec3f& A, const Vec3f& B, const Vec3f& C) {
    float Q = 0.5f * (-B[h] * C[0] + A[h] * (-B[0] + C[0]) + A[0] * (B[h] - C[h]) + B[0] * C[h]);
    float sign = Q < 0.f ? -1.f : 1.f;
    float s = (A[h] * C[0] - A[0] * C[h] + (C[h] - A[h]) * P[0] + (A[0] - C[0]) * P[h]) * sign;
    float t = (A[0] * B[h] - A[h] * B[0] + (A[h] - B[h]) * P[0] + (B[0] - A[0]) * P[h]) * sign;
    return s > 0.f && t > 0.f && (s + t) < 2.f * Q * sign;
  };

  std::vector<uint16_t> poly(vertices.size());
  std::iota(poly.begin(), poly.end(), 0);

  auto isEar = [&](const Vec3f& A, const Vec3f& B, const Vec3f& C) {
    if (!anticlockwise(A, B, C)) {
      return false;
    }
    for (auto& i : poly) {
      const Vec3f& v = vertices[i];
      if (v == A || v == B || v == C) {
        continue;
      }
      if (pointInTriangle(v, A, B, C)) {
        return false;
      }
    }
    return true;
  };

  while (poly.size() > 3) {
    for (size_t i = 1; i < poly.size(); ++i) {
      uint16_t idxA = poly[i - 1];
      uint16_t idxB = poly[i];
      uint16_t idxC = poly[(i + 1) % poly.size()];
      const Vec3f& A = vertices[idxA];
      const Vec3f& B = vertices[idxB];
      const Vec3f& C = vertices[idxC];

      if (isEar(A, B, C)) {
        indices.push_back(idxA);
        indices.push_back(idxB);
        indices.push_back(idxC);

        poly.erase(poly.begin() + i);
        break;
      }
    }
  }

  assert(poly.size() == 3);
  indices.push_back(poly[0]);
  indices.push_back(poly[1]);
  indices.push_back(poly[2]);

  return indices;
}

Mat4x4f lookAt(const Vec3f& eye, const Vec3f& centre)
{
  Mat4x4f m = identityMatrix<4>();
  Vec3f f((centre - eye).normalise()); // TODO: Wrong way round?
  Vec3f s(f.cross({ 0, 1, 0 }).normalise());
  Vec3f u(s.cross(f));
  m.set(0, 0, s[0]);
  m.set(0, 1, s[1]);
  m.set(0, 2, s[2]);
  m.set(1, 0, u[0]);
  m.set(1, 1, u[1]);
  m.set(1, 2, u[2]);
  m.set(2, 0, f[0]);
  m.set(2, 1, f[1]);
  m.set(2, 2, f[2]);
  m.set(0, 3, -s.dot(eye));
  m.set(1, 3, -u.dot(eye));
  m.set(2, 3, -f.dot(eye));
  return m;
}

Mat4x4f perspective(float fovX, float fovY, float n, float f)
{
  Mat4x4f m;
  const float t = n * tan(fovY * 0.5f);
  const float b = -t;
  const float r = n * tan(fovX * 0.5f);
  const float l = -r;

  m.set(0, 0, 2.f * n / (r - l));
  m.set(0, 2, (r + l) / (l - r));
  m.set(1, 1, -2.f * n / (t - b));
  m.set(1, 2, (t + b) / (b - t));
  m.set(2, 2, f / (f - n));
  m.set(2, 3, f * n / (n - f));
  m.set(3, 2, 1.f);
  m.set(3, 3, 0.f);

  return m;
}

Mat4x4f orthographic(float fovX, float fovY, float n, float f)
{
  Mat4x4f m;
  const float t = f * tan(fovY * 0.5f);
  const float b = -t;
  const float r = f * tan(fovX * 0.5f);
  const float l = -r;

  m.set(0, 0, 2.f / (r - l));
  m.set(0, 3, (r + l) / (l - r));
  m.set(1, 1, 2.f / (b - t));
  m.set(1, 3, (b + t) / (b - t));
  m.set(2, 2, 1.f / (f - n));
  m.set(2, 3, -n / (f - n));
  m.set(3, 3, 1.f);

  return m;
}

Mat4x4f orthographic(float l, float r, float t, float b, float n, float f)
{
  Mat4x4f m;

  m.set(0, 0, 2.f / (r - l));
  m.set(0, 3, (r + l) / (l - r));
  m.set(1, 1, 2.f / (b - t));
  m.set(1, 3, (b + t) / (b - t));
  m.set(2, 2, 1.f / (f - n));
  m.set(2, 3, -n / (f - n));
  m.set(3, 3, 1.f);

  return m;
}

Mat2x2f inverse(const Mat2x2f& M)
{
  return adjoint(M) / determinant(M);
}

inline float minor(const Mat3x3f& M, int row, int col)
{
  int r1, r2, c1, c2;

  switch (row) {
    case 0: {
      r1 = 1;
      r2 = 2;
      break;
    }
    case 1: {
      r1 = 0;
      r2 = 2;
      break;
    }
    case 2: {
      r1 = 0;
      r2 = 1;
      break;
    }
  }

  switch (col) {
    case 0: {
      c1 = 1;
      c2 = 2;
      break;
    }
    case 1: {
      c1 = 0;
      c2 = 2;
      break;
    }
    case 2: {
      c1 = 0;
      c2 = 1;
      break;
    }
  }

  return M.at(r1, c1) * M.at(r2, c2) - M.at(r2, c1) * M.at(r1, c2);
}

inline float determinant(const Mat3x3f& M)
{
  return M.at(0, 0) * minor(M, 0, 0) - M.at(0, 1) * minor(M, 0, 1) + M.at(0, 2) * minor(M, 0, 2);
}

Mat3x3f inverse(const Mat3x3f& M)
{
  Mat3x3f inv;

  float det_rp = 1.0 / determinant(M);

  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      int sign = (row + col) % 2 == 0 ? 1.0 : -1.0;

      inv.set(row, col, sign * det_rp * minor(M, row, col));
    }
  }

  return inv;
}

Mat4x4f Transform::toMatrix() const
{
  Mat4x4f m = identityMatrix<4>();
  if (scale.has_value()) {
    m = scaleMatrix4x4(scale.value());
  }
  if (rotation.has_value()) {
    m = rotationMatrix4x4(rotation.value()) * m;
  }
  if (translation.has_value()) {
    m = translationMatrix4x4(translation.value()) * m;
  }
  return m;
}

Vec3f planeIntersection(const Plane& A, const Plane& B, const Plane& C)
{
  auto M = inverse(Mat3x3f{
    -A.normal[0], -B.normal[0], -C.normal[0],
    -A.normal[1], -B.normal[1], -C.normal[1],
    -A.normal[2], -B.normal[2], -C.normal[2]
  });

  return M * Vec3f{ A.distance, B.distance, C.distance };
}

void Transform::mix(const Transform& T)
{
  if (T.rotation.has_value()) {
    //DBG_ASSERT(!rotation.has_value(), "Transform already has rotation");
    rotation = T.rotation;
  }
  if (T.translation.has_value()) {
    //DBG_ASSERT(!translation.has_value(), "Transform already has translation");
    translation = T.translation;
  }
  if (T.scale.has_value()) {
    //DBG_ASSERT(!scale.has_value(), "Transform already has scale");
    scale = T.scale;
  }
}

Mat4x4f screenSpaceTransform(const Vec2f& pos, const Vec2f& size, float rotation, Vec2f pivot)
{
  Vec3f scaledPivot{ size[0] * pivot[0], size[1] * pivot[1], 0.f };

  return
    translationMatrix4x4(Vec3f{ pos[0], pos[1], 0.f }) *
    translationMatrix4x4(scaledPivot) *
    rotationMatrix4x4(Vec3f{ 0.f, 0.f, rotation }) *
    translationMatrix4x4(-scaledPivot) *
    scaleMatrix4x4(Vec3f{ size[0], size[1], 0.f });
}

} // namespace lithic3d

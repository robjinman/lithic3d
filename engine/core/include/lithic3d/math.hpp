#pragma once

#include "exception.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>
#include <optional>
#include <cstring>
#include <array>
#include <cassert>

namespace lithic3d
{

constexpr double PId = 3.14159265359;
constexpr float PIf = 3.14159265359f;

template<typename T>
T degreesToRadians(T degrees)
{
  constexpr T x = static_cast<T>(PId / 180.0);
  return degrees * x;
}

template<typename T>
T radiansToDegrees(T radians)
{
  constexpr T x = static_cast<T>(360.0 / (2.0 * PId));
  return radians * x;
}

template<typename T>
T square(T x)
{
  return x * x;
}

template<typename T>
T clip(T value, T min, T max)
{
  return std::max(min, std::min(max, value));
}

template<typename T>
T sine(T a)
{
  return static_cast<T>(sin(static_cast<T>(a)));
}

template<typename T>
T cosine(T a)
{
  return static_cast<T>(cos(static_cast<T>(a)));
}

template<typename T, size_t N>
class Vector
{
  public:
    Vector() : m_data{}
    {
    }

    Vector(const std::initializer_list<T>& data)
    {
      ASSERT(data.size() == N, "Vector initialised with incorrect number of values");
      std::copy(data.begin(), data.end(), std::begin(m_data));
    }

    template<size_t M>
    Vector(const Vector<T, M>& rhs, const std::initializer_list<T>& rest)
    {
      static_assert(M <= N, "Expected M <= N");
      for (size_t i = 0; i < M; ++i) {
        m_data[i] = rhs[i];
      }
      ASSERT(rest.size() == N - M, "Vector initialised with incorrect number of values");
      std::copy(rest.begin(), rest.end(), std::begin(m_data) + M);
    }

    const T* data() const
    {
      return m_data;
    }

    T* data()
    {
      return m_data;
    }

    const T& operator[](size_t i) const
    {
      DBG_ASSERT(i < N, "Index out of range");
      return m_data[i];
    }

    T& operator[](size_t i)
    {
      DBG_ASSERT(i < N, "Index out of range");
      return m_data[i];
    }

    template<size_t M>
    Vector<T, M> sub() const
    {
      static_assert(M <= N, "Expected M <= N");

      Vector<T, M> v;
      for (size_t i = 0; i < M; ++i) {
        v[i] = m_data[i];
      }
      return v;
    }

    template<typename U>
    operator Vector<U, N>() const
    {
      Vector<U, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = static_cast<U>(m_data[i]);
      }
      return v;
    }

    Vector<T, N> operator-() const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = -m_data[i];
      }
      return v;
    }

    Vector<T, N> operator+(const Vector<T, N>& rhs) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] + rhs[i];
      }
      return v;
    }

    Vector<T, N> operator-(const Vector<T, N>& rhs) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] - rhs[i];
      }
      return v;
    }

    Vector<T, N>& operator+=(const Vector<T, N>& rhs)
    {
      for (size_t i = 0; i < N; ++i) {
        m_data[i] += rhs.m_data[i];
      }
      return *this;
    }

    Vector<T, N>& operator-=(const Vector<T, N>& rhs)
    {
      for (size_t i = 0; i < N; ++i) {
        m_data[i] -= rhs.m_data[i];
      }
      return *this;
    }

    Vector<T, N> operator*(T s) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] * s;
      }
      return v;
    }

    Vector<T, N> operator/(T s) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] / s;
      }
      return v;
    }

    Vector<T, N> operator*(const Vector<T, N>& rhs) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] * rhs.m_data[i];
      }
      return v;
    }

    Vector<T, N> operator/(const Vector<T, N>& rhs) const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v[i] = m_data[i] / rhs.m_data[i];
      }
      return v;
    }

    T squareMagnitude() const
    {
      T sqSum = 0;
      for (size_t i = 0; i < N; ++i) {
        sqSum += m_data[i] * m_data[i];
      }
      return sqSum;
    }

    T magnitude() const
    {
      return sqrt(squareMagnitude());
    }

    Vector<T, N> normalise() const
    {
      T mag = magnitude();
      return mag != 0.0 ? (*this) / magnitude() : (*this);
    }

    Vector<T, N> inverse() const
    {
      Vector<T, N> v;
      for (size_t i = 0; i < N; ++i) {
        v.m_data[i] = -m_data[i];
      }
      return v;
    }

    bool operator==(const Vector<T, N>& rhs) const
    {
      for (size_t i = 0; i < N; ++i) {
        if (m_data[i] != rhs.data()[i]) {
          return false;
        }
      }
      return true;
    }

    bool operator!=(const Vector<T, N>& rhs) const
    {
      return !(*this == rhs);
    }

    bool operator<(const Vector<T, N>& rhs) const
    {
      return squareMagnitude() < rhs.squareMagnitude();
    }

    T dot(const Vector<T, N>& rhs) const
    {
      T sum = 0;
      for (size_t i = 0; i < N; ++i) {
        sum += m_data[i] * rhs.m_data[i];
      }
      return sum;
    }

    Vector<T, 3> cross(const Vector<T, 3>& rhs) const
    {
      return Vector<T, N>{
        m_data[1] * rhs.m_data[2] - m_data[2] * rhs.m_data[1],
        m_data[2] * rhs.m_data[0] - m_data[0] * rhs.m_data[2],
        m_data[0] * rhs.m_data[1] - m_data[1] * rhs.m_data[0]
      };
    }

  private:
    T m_data[N];
};

template<typename T, size_t N>
std::ostream& operator<<(std::ostream& stream, const Vector<T, N>& v)
{
  for (size_t i = 0; i < N; ++i) {
    stream << v[i] << (i + 1 < N ? ", " : "");
  }
  return stream;
}

template<typename T, size_t ROWS, size_t COLS>
class Matrix
{
  public:
    Matrix() : m_data{}
    {
    }

    Matrix(const std::initializer_list<T>& data)
    {
      if (data.size() != ROWS * COLS) {
        EXCEPTION("Expected initializer list should of length " << ROWS * COLS);
      }

      // Transpose and store
      size_t i = 0;
      for (T x : data) {
        set(i / COLS, i % COLS, x);
        ++i;
      }
    }

    Matrix(const Matrix<T, ROWS, COLS>& cp)
    {
      std::copy(std::begin(cp.m_data), std::end(cp.m_data), std::begin(m_data));
    }

    Matrix<T, ROWS, COLS>& operator=(const Matrix<T, ROWS, COLS>& rhs)
    {
      std::copy(std::begin(rhs.m_data), std::end(rhs.m_data), std::begin(m_data));
      return *this;
    }

    void assign(const std::array<T, ROWS * COLS>& data)
    {
      std::memcpy(m_data, data.data(), data.size() * sizeof(T));
    }

    const T* data() const
    {
      return m_data;
    }

    T* data()
    {
      return m_data;
    }

    const T& at(size_t row, size_t col) const
    {
      DBG_ASSERT(row < ROWS, "Index out of range");
      DBG_ASSERT(col < COLS, "Index out of range");
      return m_data[col * ROWS + row];
    }

    void set(size_t row, size_t col, T value)
    {
      DBG_ASSERT(row < ROWS, "Index out of range");
      DBG_ASSERT(col < COLS, "Index out of range");
      m_data[col * ROWS + row] = value;
    }

    template<typename U>
    operator Matrix<U, ROWS, COLS>() const
    {
      Matrix<U, ROWS, COLS> m;
      for (size_t r = 0; r < ROWS; ++r) {
        for (size_t c = 0; c < COLS; ++c) {
          m.set(r, c, static_cast<U>(at(r, c)));
        }
      }
      return m;
    }

    Matrix<T, ROWS, COLS> operator+(const Matrix<T, ROWS, COLS>& rhs) const
    {
      Matrix<T, ROWS, COLS> m;
      for (size_t i = 0; i < ROWS * COLS; ++i) {
        m.m_data[i] = m_data[i] + rhs.m_data[i];
      }
      return m;
    }

    Matrix<T, ROWS, COLS>& operator+=(const Matrix<T, ROWS, COLS>& rhs)
    {
      for (size_t i = 0; i < ROWS * COLS; ++i) {
        m_data[i] += rhs.m_data[i];
      }
      return *this;
    }

    Matrix<T, ROWS, COLS> operator*(T s) const
    {
      Matrix<T, ROWS, COLS> m;
      for (size_t i = 0; i < ROWS * COLS; ++i) {
        m.m_data[i] = m_data[i] * s;
      }
      return m;
    }

    Matrix<T, ROWS, COLS> operator/(T s) const
    {
      Matrix<T, ROWS, COLS> m;
      for (size_t i = 0; i < ROWS * COLS; ++i) {
        m.m_data[i] = m_data[i] / s;
      }
      return m;
    }

    Vector<T, ROWS> operator*(const Vector<T, COLS>& rhs) const
    {
      Vector<T, ROWS> v;
      for (size_t j = 0; j < ROWS; ++j) {
        T sum = 0;
        for (size_t i = 0; i < COLS; ++i) {
          sum += at(j, i) * rhs[i];
        }
        v[j] = sum;
      }
      return v;
    }

    template <size_t RCOLS>
    Matrix<T, ROWS, RCOLS> operator*(const Matrix<T, COLS, RCOLS>& rhs) const
    {
      Matrix<T, ROWS, RCOLS> m;
      for (size_t j = 0; j < ROWS; ++j) {
        for (size_t i = 0; i < RCOLS; ++i) {
          T sum = 0;
          for (size_t k = 0; k < COLS; ++k) {
            sum += at(j, k) * rhs.at(k, i);
          }
          m.set(j, i, sum);
        }
      }
      return m;
    }

    Matrix<T, COLS, ROWS> t() const
    {
      Matrix<T, COLS, ROWS> m;
      for (size_t i = 0; i < COLS; ++i) {
        for (size_t j = 0; j < ROWS; ++j) {
          m.set(i, j, at(j, i));
        }
      }
      return m;
    }

    bool operator==(const Matrix<T, ROWS, COLS>& rhs) const
    {
      for (size_t i = 0; i < ROWS * COLS; ++i) {
        if (m_data[i] != rhs.data()[i]) {
          return false;
        }
      }
      return true;
    }

    Vector<T, COLS> row(size_t r) const
    {
      Vector<T, COLS> v;
      for (size_t i = 0; i < COLS; ++i) {
        v[i] = at(r, i);
      }
      return v;
    }

  private:
    T m_data[ROWS * COLS];
};

template<typename T, size_t ROWS, size_t COLS>
std::ostream& operator<<(std::ostream& stream, const Matrix<T, ROWS, COLS>& m)
{
  for (size_t r = 0; r < ROWS; ++r) {
    for (size_t c = 0; c < COLS; ++c) {
      stream << m.at(r, c) << (c + 1 < COLS ? ", " : "");
    }
    stream << "\n";
  }
  return stream;
}

using Vec2i = Vector<int, 2>;
using Vec2u = Vector<uint32_t, 2>;
using Vec2f = Vector<float, 2>;
using Vec3f = Vector<float, 3>;
using Vec3i = Vector<int, 3>;
using Vec4f = Vector<float, 4>;
using Mat2x2f = Matrix<float, 2, 2>;
using Mat3x3f = Matrix<float, 3, 3>;
using Mat2x3f = Matrix<float, 2, 3>;
using Mat3x2f = Matrix<float, 3, 2>;
using Mat4x4f = Matrix<float, 4, 4>;

template<size_t M>
Matrix<float, M, M> scaleMatrix(float scale, bool homogeneous)
{
  Matrix<float, M, M> m;
  for (size_t i = 0; i < M; ++i) {
    m.set(i, i, scale);
  }
  if (homogeneous) {
    m.set(M - 1, M - 1, 1);
  }
  return m;
};

inline Mat4x4f scaleMatrix4x4(const Vec3f& scale)
{
  return Mat4x4f{
    scale[0], 0.f, 0.f, 0.f,
    0.f, scale[1], 0.f, 0.f,
    0.f, 0.f, scale[2], 0.f,
    0.f, 0.f, 0.f, 1.f
  };
}

template<size_t M>
Matrix<float, M, M> identityMatrix()
{
  return scaleMatrix<M>(1, false);
}

inline Mat3x3f crossProductMatrix3x3(const Vec3f& k)
{
  return Mat3x3f{
    0, -k[2], k[1],
    k[2], 0, -k[0],
    -k[1], k[0], 0
  };
}

// Rodrigues' rotation formula
inline Mat3x3f rotationMatrix3x3(const Vec3f& k, float theta)
{
  auto K = crossProductMatrix3x3(k);
  return identityMatrix<3>() + K * sin(theta) + (K * K) * (1.f - cos(theta));
}

// Rodrigues' rotation formula
inline Mat4x4f rotationMatrix4x4(const Vec3f& k, float theta)
{
  auto m = rotationMatrix3x3(k, theta);
  return {
    m.at(0, 0), m.at(0, 1), m.at(0, 2), 0.f,
    m.at(1, 0), m.at(1, 1), m.at(1, 2), 0.f,
    m.at(2, 0), m.at(2, 1), m.at(2, 2), 0.f,
    0.f, 0.f, 0.f, 1.f
  };
}

// From Euler angles
inline Mat3x3f rotationMatrix3x3(const Vec3f& ori)
{
  Mat3x3f X{
    1, 0, 0,
    0, cosine(ori[0]), -sine(ori[0]),
    0, sine(ori[0]), cosine(ori[0])
  };
  Mat3x3f Y{
    cosine(ori[1]), 0, sine(ori[1]),
    0, 1, 0,
    -sine(ori[1]), 0, cosine(ori[1])
  };
  Mat3x3f Z{
    cosine(ori[2]), -sine(ori[2]), 0,
    sine(ori[2]), cosine(ori[2]), 0,
    0, 0, 1
  };
  return Z * (Y * X);
}

inline Mat4x4f translationMatrix4x4(const Vec3f& pos)
{
  auto m = identityMatrix<4>();
  m.set(0, 3, pos[0]);
  m.set(1, 3, pos[1]);
  m.set(2, 3, pos[2]);
  return m;
}

inline Mat4x4f rotationMatrix4x4(const Vec3f& ori)
{
  auto rot = rotationMatrix3x3(ori);
  Mat4x4f m = identityMatrix<4>();
  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 3; ++c) {
      m.set(r, c, rot.at(r, c));
    }
  }
  return m;
}

inline Mat4x4f rotationMatrix4x4(const Vec4f& quaternion)
{
  float w = quaternion[0];
  float x = quaternion[1];
  float y = quaternion[2];
  float z = quaternion[3];

  return Mat4x4f{
    1.f - 2.f * y * y - 2.f * z * z, 2.f * x * y - 2.f * z * w, 2.f * x * z + 2.f * y * w, 0.f,
    2.f * x * y + 2.f * z * w, 1.f - 2.f * x * x - 2.f * z * z, 2.f * y * z - 2.f * x * w, 0.f,
    2.f * x * z - 2.f * y * w, 2.f * y * z + 2.f * x * w, 1.f - 2.f * x * x - 2.f * y * y, 0.f,
    0.f, 0.f, 0.f, 1.f
  };
}

inline Mat4x4f createTransform(const Vec3f& pos, const Vec3f& ori, const Vec3f& scale)
{
  return translationMatrix4x4(pos) * rotationMatrix4x4(ori) * scaleMatrix4x4(scale);
}

inline Mat4x4f createTransform(const Vec3f& pos, const Vec3f& ori)
{
  return translationMatrix4x4(pos) * rotationMatrix4x4(ori);
}

inline Mat4x4f createTransform(const Mat3x3f& rot, const Vec3f& transl)
{
  return {
    rot.at(0, 0), rot.at(0, 1), rot.at(0, 2), transl[0],
    rot.at(1, 0), rot.at(1, 1), rot.at(1, 2), transl[1],
    rot.at(2, 0), rot.at(2, 1), rot.at(2, 2), transl[2],
    0.f, 0.f, 0.f, 1.f
  };
}

inline Mat4x4f applyRotation(const Mat4x4f& M, const Mat3x3f& R)
{
  Mat4x4f K;
  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 3; ++c) {
      K.set(r, c, R.at(r, 0) * M.at(0, c) + R.at(r, 1) * M.at(1, c) + R.at(r, 2) * M.at(2, c));
    }
    K.set(r, 3, M.at(r, 3));
  }
  K.set(3, 3, 1.f);
  return K;
}

inline Mat4x4f fromVerticalToVectorTransform(const Vec3f& vec)
{
  Vec3f u = vec.normalise();
  Vec3f w{ 0, 0, 1 };
  Vec3f v = w.cross(u).normalise();
  w = u.cross(v);

  return Mat4x4f{
    v[0], w[0], u[0], 0,
    v[1], w[1], u[1], 0,
    v[2], w[2], u[2], 0,
    0, 0, 0, 1
  };
}

inline Mat3x3f getRotation3x3(const Mat4x4f& m)
{
  Mat3x3f rot;
  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 3; ++c) {
      rot.set(r, c, m.at(r, c));
    }
  }
  return rot;
}

inline Vec3f getTranslation(const Mat4x4f& m)
{
  return Vec3f{m.at(0, 3), m.at(1, 3), m.at(2, 3)};
}

inline Vec3f getDirection(const Mat4x4f& m)
{
  return (getRotation3x3(m) * Vec3f{ 0.f, 0.f, -1.f }).normalise();
}

inline void setTranslation(Mat4x4f& m, const Vec3f& t)
{
  m.set(0, 3, t[0]);
  m.set(1, 3, t[1]);
  m.set(2, 3, t[2]);
}

struct LineSegment
{
  Vec2f A;
  Vec2f B;
};

struct Line
{
  Line(const Vec2f& A, const Vec2f& B);
  Line(float a, float b, float c);

  float a;
  float b;
  float c;
};

struct Transform
{
  std::optional<Vec4f> rotation;
  std::optional<Vec3f> translation;
  std::optional<Vec3f> scale;

  Mat4x4f toMatrix() const;
  void mix(const Transform& T);
};

template<typename T>
struct Rect
{
  T x;
  T y;
  T w;
  T h;

  bool isInside(const Vector<T, 2>& p)
  {
    return p[0] >= x && p[0] <= x + w && p[1] >= y && p[1] <= y + h;
  }
};

template<typename T>
std::ostream& operator<<(std::ostream& stream, const Rect<T>& r)
{
  stream << "{ x: " << r.x << ", y: " << r.y << ", w: " << r.w << ", h: " << r.h << " }";
  return stream;
}

// Ax + By + Cz + D = 0
struct Plane
{
  Vec3f normal;     // A, B, C
  float distance;   // D

  void normalise()
  {
    auto magnitude = normal.magnitude();
    distance /= magnitude;
    normal = normal / magnitude;
  }
};

namespace FrustumPlane
{
  enum : size_t {
    Left = 0,
    Right,
    Top,
    Bottom,
    Near,
    Far
  };
}
using Frustum = std::array<Plane, 6>;

bool dbg_isValidFrustum(const Frustum& f);

using Rectf = Rect<float>;
using Recti = Rect<int>;

Mat4x4f lookAt(const Vec3f& eye, const Vec3f& centre);
Mat4x4f perspective(float fovY, float aspectRatio, float near, float far);
Mat4x4f orthographic(float l, float r, float t, float b, float n, float f);
bool lineIntersect(const Line& l1, const Line& l2, Vec2f& p);
Vec2f projectionOntoLine(const Line& line, const Vec2f& p);
bool lineSegmentCircleIntersect(const LineSegment& lseg, const Vec2f& centre, float radius);
bool pointIsInsidePoly(const Vec2f& p, const std::vector<Vec2f>& poly);
std::vector<uint16_t> triangulatePoly(const std::vector<Vec3f>& vertices);
Mat2x2f inverse(const Mat2x2f& M);
Mat3x3f inverse(const Mat3x3f& M);
Mat4x4f inverse(const Mat4x4f& M);
Mat4x4f screenSpaceTransform(const Vec2f& pos, const Vec2f& size, float rotation, Vec2f pivot);
Vec3f planeIntersection(const Plane& A, const Plane& B, const Plane& C);
bool intersectsFrustum(const Frustum& frustum, const Vec3f& pos, float radius);
Frustum computeFrustumFromMatrix(const Mat4x4f& m);

inline Mat4x4f screenSpaceTransform(const Vec2f& pos, const Vec2f& size)
{
  return screenSpaceTransform(pos, size, 0.f, Vec2f{});
}

} // namespace lithic3d

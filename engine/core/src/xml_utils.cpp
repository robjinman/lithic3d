#include "lithic3d/xml_utils.hpp"
#include "lithic3d/xml.hpp"

namespace lithic3d
{

Vec3f constructVec3f(const XmlNode& node)
{
  return {
    std::stof(node.attribute("x")),
    std::stof(node.attribute("y")),
    std::stof(node.attribute("z"))
  };
}

Aabb constructAabb(const XmlNode& aabbXml)
{
  return Aabb{
    .min = metresToWorldUnits(constructVec3f(*aabbXml.child("min"))),
    .max = metresToWorldUnits(constructVec3f(*aabbXml.child("max")))
  };
}

Mat4x4f constructTransform(const XmlNode& transformXml)
{
  auto iMatrix = transformXml.child("matrix");
  if (iMatrix != transformXml.end()) {
    auto& matrixXml = *iMatrix;
    std::stringstream ss(matrixXml.value());

    Mat4x4f m = identityMatrix<4>();

    size_t i = 0;
    while (ss.good()) {
      if (i > 12) {
        break;
      }

      std::string strFloat;
      std::getline(ss, strFloat, ',');
      size_t r = i / 4;
      size_t c = i % 4;

      float scaleFactor = (c == 3 && r < 3) ? WORLD_UNITS_PER_METRE : 1.f;

      m.set(r, c, std::stof(strFloat) * scaleFactor);
      ++i;
    }

    return m;
  }
  else {
    auto pos = constructVec3f(*transformXml.child("pos"));
    auto degs = constructVec3f(*transformXml.child("ori"));

    Vec3f ori{ degreesToRadians(degs[0]), degreesToRadians(degs[1]), degreesToRadians(degs[2]) };

    Vec3f scale{ 1.f, 1.f, 1.f };

    auto i = transformXml.child("scale");
    if (i != transformXml.end()) {
      scale = constructVec3f(*i);
    }

    return createTransform(metresToWorldUnits(pos), ori, scale);
  }
}

} // namespace lithic3d

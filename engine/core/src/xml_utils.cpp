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

Mat4x4f constructTransform(const XmlNode& xmlTransform)
{
  auto iMatrix = xmlTransform.child("matrix");
  if (iMatrix != xmlTransform.end()) {
    auto& xmlMatrix = *iMatrix;
    std::stringstream ss(xmlMatrix.value());

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
    auto pos = constructVec3f(*xmlTransform.child("pos"));
    auto degs = constructVec3f(*xmlTransform.child("ori"));

    Vec3f ori{ degreesToRadians(degs[0]), degreesToRadians(degs[1]), degreesToRadians(degs[2]) };

    Vec3f scale{ 1.f, 1.f, 1.f };

    auto i = xmlTransform.child("scale");
    if (i != xmlTransform.end()) {
      scale = constructVec3f(*i);
    }

    return createTransform(metresToWorldUnits(pos), ori, scale);
  }
}

XmlNodePtr toXml(const Mat4x4f& m)
{
  auto xmlTransform = createXmlNode("transform");
  auto xmlMatrix = createXmlNode("matrix");

  std::stringstream ss;

  for (size_t r = 0; r < 3; ++r) {
    for (size_t c = 0; c < 4; ++c) {
      float scaleFactor = (c == 3 && r < 3) ? 1.f / WORLD_UNITS_PER_METRE : 1.f;

      ss << m.at(r, c) * scaleFactor;
      if (!(r == 2 && c == 3)) {
        ss << ",";
      }
    }
  }

  xmlMatrix->setValue(ss.str());

  xmlTransform->addChild(std::move(xmlMatrix));

  return xmlTransform;
}

} // namespace lithic3d

#pragma once

#include "sys_spatial.hpp"

namespace lithic3d
{

class XmlNode;

Vec3f constructVec3f(const XmlNode& node);
Aabb constructAabb(const XmlNode& aabbXml);
Mat4x4f constructTransform(const XmlNode& transformXml);

} // namespace lithic3d

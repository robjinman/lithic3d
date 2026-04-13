#pragma once

#include "lithic3d/file_system.hpp"
#include <memory>
#include <string>

namespace lithic3d
{

class Drm
{
  public:
    virtual bool isActivated() const = 0;
    virtual bool activate(const std::string& key) = 0;

    virtual ~Drm() = default;
};

using DrmPtr = std::unique_ptr<Drm>;

class Logger;

DrmPtr createDrm(const std::string& productName, DirectoryPtr userDataDir, Logger& logger);

} // namespace lithic3d

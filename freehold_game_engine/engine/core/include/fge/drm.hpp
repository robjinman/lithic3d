#pragma once

#include <memory>

namespace fge
{

class Drm
{
  public:
    virtual bool isActivated() const = 0;
    virtual bool activate(const std::string& key) = 0;

    virtual ~Drm() = default;
};

using DrmPtr = std::unique_ptr<Drm>;

class FileSystem;

DrmPtr createDrm(FileSystem& fileSystem);

} // namespace fge

#pragma once

#include <filesystem>
#include <memory>

namespace lithic3d
{

class PlatformPaths
{
  public:
    virtual const std::filesystem::path& appData() const = 0;
    virtual const std::filesystem::path& userData() const = 0;

    virtual ~PlatformPaths() = default;
};

using PlatformPathsPtr = std::unique_ptr<PlatformPaths>;

} // namespace lithic3d

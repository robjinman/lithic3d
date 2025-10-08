#pragma once

#include <filesystem>
#include <memory>

class PlatformPaths
{
  public:
    virtual const std::filesystem::path& appData() const = 0;
    virtual const std::filesystem::path& userData() const = 0;

    virtual ~PlatformPaths() = default;
};

using PlatformPathsPtr = std::unique_ptr<PlatformPaths>;

#pragma once

#include <string>

namespace fge
{

enum class Platform
{
  Windows,
  Linux,
  OSX,
  iOS,
  Android
};

#if defined(PLATFORM_WINDOWS)
const Platform PLATFORM = Platform::Windows;
const std::string PLATFORM_NAME = "windows";
const bool MOBILE_PLATFORM = false;
#elif defined(PLATFORM_OSX)
const Platform PLATFORM = Platform::OSX;
const std::string PLATFORM_NAME = "osx";
const bool MOBILE_PLATFORM = false;
#elif defined(PLATFORM_ANDROID)
const Platform PLATFORM = Platform::Android;
const std::string PLATFORM_NAME = "android";
const bool MOBILE_PLATFORM = true;
#elif defined(PLATFORM_IOS)
const Platform PLATFORM = Platform::iOS;
const std::string PLATFORM_NAME = "ios";
const bool MOBILE_PLATFORM = true;
#else // PLATFORM_LINUX
const Platform PLATFORM = Platform::Linux;
const std::string PLATFORM_NAME = "linux";
const bool MOBILE_PLATFORM = false;
#endif

} // namespace fge

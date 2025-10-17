#pragma once

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
const bool MOBILE_PLATFORM = false;
#elif defined(PLATFORM_LINUX)
const Platform PLATFORM = Platform::Linux;
const bool MOBILE_PLATFORM = false;
#elif defined(PLATFORM_OSX)
const Platform PLATFORM = Platform::OSX;
const bool MOBILE_PLATFORM = false;
#elif defined(PLATFORM_ANDROID)
const Platform PLATFORM = Platform::Android;
const bool MOBILE_PLATFORM = true;
#elif defined(PLATFORM_IOS)
const Platform PLATFORM = Platform::IOS;
const bool MOBILE_PLATFORM = true;
#else
const Platform PLATFORM = Platform::Linux;
const bool MOBILE_PLATFORM = false;
#endif

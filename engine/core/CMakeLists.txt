cmake_minimum_required(VERSION 3.22)

if(CMAKE_SYSTEM_NAME MATCHES "Android")
  set(PLATFORM_ANDROID 1)
  set(PLATFORM_ANDROID "${PLATFORM_ANDROID}" PARENT_SCOPE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(PLATFORM_LINUX 1)
  set(PLATFORM_LINUX "${PLATFORM_LINUX}" PARENT_SCOPE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(PLATFORM_WINDOWS 1)
  set(PLATFORM_WINDOWS "${PLATFORM_WINDOWS}" PARENT_SCOPE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(PLATFORM_OSX 1)
  set(PLATFORM_OSX "${PLATFORM_OSX}" PARENT_SCOPE)
elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
  set(PLATFORM_IOS 1)
  set(PLATFORM_IOS "${PLATFORM_IOS}" PARENT_SCOPE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

if(PLATFORM_ANDROID)
  if(NOT DEFINED ENV{ANDROID_NDK_HOME})
    message(FATAL_ERROR "Please set ANDROID_NDK_HOME")
    return()
  endif()

  set(VCPKG_MANIFEST_DIR "${REPO_ROOT}/engine/platform/android/vcpkg")
  message("${VCPKG_MANIFEST_DIR}")
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(VCPKG_INSTALLED_DIR "${REPO_ROOT}/build/android/vcpkg_installed")
  set(CMAKE_SYSTEM_NAME "android")
  set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
  set(VCPKG_TARGET_TRIPLET "arm64-android") # TODO
  set(VCPKG_CMAKE_SYSTEM_NAME "Android")
  set(ENV{VULKAN_SDK} "$ENV{ANDROID_NDK_HOME}/sysroot/usr")
  set(ANDROID_ABI "arm64-v8a")  # TODO
endif()

# Must come AFTER setting VCPKG_... variables
project("FreeholdGameEngineCore")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_MAJOR "${VERSION_MAJOR}" PARENT_SCOPE)
set(VERSION_MINOR "${VERSION_MINOR}" PARENT_SCOPE)

set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}")
set(VERSION_STRING "${VERSION_STRING}" PARENT_SCOPE)

#string(TIMESTAMP BUILD_ID)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("System name is ${CMAKE_SYSTEM_NAME}")
message("Building ${CMAKE_BUILD_TYPE} configuration...")

set(ENGINE_LIB_TARGET "core")
set(ENGINE_LIB_TARGET "${ENGINE_LIB_TARGET}" PARENT_SCOPE)

find_package(tinyxml2 REQUIRED)
find_package(Stb REQUIRED)
find_package(OpenAL CONFIG REQUIRED)
find_package(fmt REQUIRED)  # For OpenAL

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version_in.hpp"
  "${PROJECT_BINARY_DIR}/include/version.hpp"
)

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/*.cpp"
)

if(PLATFORM_LINUX)
  if (CPU_PROFILE)
    message("CPU profiling ON")
    #find_package(GOOGLE_PERFTOOLS REQUIRED)
  endif()

  find_package(Vulkan REQUIRED)
  find_package(OpenSSL REQUIRED)
  set(COMPILE_FLAGS -Wextra -Wall -DDRM -DPLATFORM_LINUX)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g -D_GLIBCXX_DEBUG)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
  if (CPU_PROFILE)
    set(RELEASE_COMPILE_FLAGS ${RELEASE_COMPILE_FLAGS} -g)
  endif()
  add_library(${ENGINE_LIB_TARGET} ${CPP_SOURCES})
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC
    Vulkan::Vulkan
    OpenSSL::SSL
    OpenAL::OpenAL
    fmt::fmt
  )
elseif(PLATFORM_ANDROID)
  find_package(Vulkan REQUIRED)
  set(COMPILE_FLAGS -Wextra -Wall -DPLATFORM_ANDROID)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
  add_library(${ENGINE_LIB_TARGET} STATIC
    ${CPP_SOURCES}
  )
  add_library(app-glue STATIC
    "$ENV{ANDROID_NDK_HOME}/sources/android/native_app_glue/android_native_app_glue.c"
  )
  target_include_directories(app-glue PUBLIC
    "$ENV{ANDROID_NDK_HOME}/sources/android/native_app_glue"
  )
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC
    android
    log
    app-glue
    Vulkan::Vulkan
    OpenAL::OpenAL
    fmt::fmt
  )
  set(GAME_PROJECT_DIRECTORY "${REPO_ROOT}/minefield") # TODO
  add_subdirectory("${GAME_PROJECT_DIRECTORY}" "game")
  # Export ANativeActivity_onCreate(),
  # Refer to: https://github.com/android-ndk/ndk/issues/381.
  set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
  add_library(app SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/android/src/main/cpp/application.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/android/src/main/cpp/file_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/android/src/main/cpp/logger.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/default/audio_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/default/stb_vorbis.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/default/vulkan_window_delegate.cpp"
  )
  # game will depend on core
  target_link_libraries(app PUBLIC
    game
  )
elseif(PLATFORM_WINDOWS)
  find_package(Vulkan REQUIRED)
  find_package(OpenSSL REQUIRED)
  set(COMPILE_FLAGS /W4 /DDRM /DPLATFORM_WINDOWS)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS})
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} /O2)
  add_library(${ENGINE_LIB_TARGET} ${CPP_SOURCES})
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC
    Vulkan::Vulkan
    OpenSSL::SSL
    OpenAL::OpenAL
    fmt::fmt
  )
elseif(PLATFORM_OSX)
  find_package(Vulkan REQUIRED)
  find_package(OpenSSL REQUIRED)
  set(COMPILE_FLAGS -Wextra -Wall -DDRM -DPLATFORM_OSX)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
  add_library(${ENGINE_LIB_TARGET} ${CPP_SOURCES})
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC
    Vulkan::Vulkan
    OpenSSL::SSL
    OpenAL::OpenAL
    fmt::fmt
  )
elseif(PLATFORM_IOS)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../platform/ios/cmake")
  find_package(Vulkan REQUIRED COMPONENTS MoltenVK)
  set(COMPILE_FLAGS -Wextra -Wall -DPLATFORM_IOS)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
  add_library(${ENGINE_LIB_TARGET} ${CPP_SOURCES})
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC Vulkan::MoltenVK)

  # Comment out for iOS simulator
  target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC OpenAL::OpenAL fmt::fmt)
endif()

target_include_directories(${ENGINE_LIB_TARGET}
  PRIVATE
    "${PROJECT_BINARY_DIR}/include" # Location of version.hpp
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${Stb_INCLUDE_DIR}"
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${ENGINE_LIB_TARGET} PUBLIC
  tinyxml2::tinyxml2
)

target_compile_options(${ENGINE_LIB_TARGET} PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${ENGINE_LIB_TARGET} PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

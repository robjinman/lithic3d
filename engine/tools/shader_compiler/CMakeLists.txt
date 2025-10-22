cmake_minimum_required(VERSION 3.22)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(PLATFORM_LINUX 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(PLATFORM_WINDOWS 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(PLATFORM_OSX 1)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)

set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("System name is ${CMAKE_SYSTEM_NAME}")
message("Building ${CMAKE_BUILD_TYPE} configuration...")

set(TARGET "shadercompiler")

find_package(tinyxml2 REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version_in.hpp"
  "${PROJECT_BINARY_DIR}/include/version.hpp"
)

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(PLATFORM_LINUX)
  set(COMPILE_FLAGS -Wextra -Wall -DPLATFORM_LINUX)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g -D_GLIBCXX_DEBUG)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
elseif(PLATFORM_WINDOWS)
  set(COMPILE_FLAGS /W4 /DDRM /DPLATFORM_WINDOWS)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS})
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} /O2)
elseif(PLATFORM_OSX)
  set(COMPILE_FLAGS -Wextra -Wall -DDRM -DPLATFORM_OSX)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
endif()

add_executable(${TARGET} ${CPP_SOURCES})
target_link_libraries(${TARGET} PRIVATE Vulkan::shaderc_combined tinyxml2::tinyxml2)

target_include_directories(${TARGET} PUBLIC
  "${PROJECT_BINARY_DIR}/include" # Location of version.hpp
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(${TARGET} PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${TARGET} PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

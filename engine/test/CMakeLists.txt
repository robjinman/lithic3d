cmake_minimum_required(VERSION 3.22)

find_package(GTest REQUIRED)

file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(unitTests ${SRCS})

target_link_libraries(unitTests ${ENGINE_LIB_TARGET} GTest::gtest_main GTest::gmock_main)

if(PLATFORM_LINUX)
  set(COMPILE_FLAGS -Wextra -Wall)
  set(DEBUG_COMPILE_FLAGS ${COMPILE_FLAGS} -g)
  set(RELEASE_COMPILE_FLAGS ${COMPILE_FLAGS} -O3)
elseif(PLATFORM_WINDOWS)
  # TODO
elseif(PLATFORM_OSX)
  # TODO
endif()

target_compile_options(unitTests PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(unitTests PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

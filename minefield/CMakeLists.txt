cmake_minimum_required(VERSION 3.22)

# Build a static library target called 'game' with a dependency on ${ENGINE_LIB_TARGET}

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version_in.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/include/version.hpp"
)

file(GLOB CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(game ${CPP_SOURCES})

target_include_directories(game PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}/include" # Location of version.hpp
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(game PUBLIC "${ENGINE_LIB_TARGET}")

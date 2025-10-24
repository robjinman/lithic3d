cmake_minimum_required(VERSION 3.22)

#########################################
# Edit accordingly

set(GAME_BUNDLE_IDENTIFIER "com.freeholdapps.minefield")
set(GAME_FULL_NAME "com.freeholdapps.minefield")
set(GAME_SHORT_NAME "minefield")
set(GAME_VERSION_MAJOR "2")
set(GAME_VERSION_MINOR "0")

set(GAME_BUNDLE_IDENTIFIER "${GAME_BUNDLE_IDENTIFIER}" PARENT_SCOPE)
set(GAME_FULL_NAME "${GAME_FULL_NAME}" PARENT_SCOPE)
set(GAME_SHORT_NAME "${GAME_SHORT_NAME}" PARENT_SCOPE)
set(GAME_VERSION_MAJOR "${GAME_VERSION_MAJOR}" PARENT_SCOPE)
set(GAME_VERSION_MINOR "${GAME_VERSION_MINOR}" PARENT_SCOPE)

#########################################
# Build a library target called 'game' with a dependency on ${ENGINE_LIB_TARGET}

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

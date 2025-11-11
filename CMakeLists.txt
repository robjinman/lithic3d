cmake_minimum_required(VERSION 3.22)

project("Lithic3DProject")

set(GAME_PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}/$ENV{LITHIC3D_PROJECT}")
include("${GAME_PROJECT_DIRECTORY}/game.cmake")

add_subdirectory(engine)

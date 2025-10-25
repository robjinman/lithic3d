cmake_minimum_required(VERSION 3.22)

project("FreeholdGameEngineProject")

set(GAME_PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}/$ENV{FGE_PROJECT}")
include("${GAME_PROJECT_DIRECTORY}/game.cmake")

add_subdirectory(engine)

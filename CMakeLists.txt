cmake_minimum_required(VERSION 3.22)

project("FreeholdGameProject")

# TODO
set(GAME_PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}/minefield")
include("${GAME_PROJECT_DIRECTORY}/game.cmake")

add_subdirectory(engine)

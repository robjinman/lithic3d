cmake_minimum_required(VERSION 3.22)

# Build a static library target called 'game' with a public dependency on ${ENGINE_LIB_TARGET}

file(GLOB CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(game ${CPP_SOURCES})

target_link_libraries(game PUBLIC "${ENGINE_LIB_TARGET}")

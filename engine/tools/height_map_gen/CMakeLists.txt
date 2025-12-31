cmake_minimum_required(VERSION 3.22)

set(TARGET "height_map_gen")

find_package(Stb REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${TARGET} ${CPP_SOURCES})

target_include_directories(${TARGET} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${Stb_INCLUDE_DIR}"
)

target_link_libraries(${TARGET} PRIVATE Boost::program_options)

target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

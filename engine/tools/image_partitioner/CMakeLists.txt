cmake_minimum_required(VERSION 3.22)

set(LIB_TARGET "image_partitioner_lib")
set(EXE_TARGET "image_partitioner")

set(IMAGE_PARTITIONER_LIB_TARGET "${LIB_TARGET}" PARENT_SCOPE)

find_package(Stb REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

file(GLOB LIB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB EXE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/cli/*.cpp"
)

add_library(${LIB_TARGET} ${LIB_SOURCES})

target_link_libraries(${LIB_TARGET}
  PUBLIC
    ${ENGINE_LIB_TARGET}
)

target_include_directories(${LIB_TARGET}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${Stb_INCLUDE_DIR}"
)

target_compile_options(${LIB_TARGET} PUBLIC "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${LIB_TARGET} PUBLIC "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

add_executable(${EXE_TARGET} ${EXE_SOURCES})

target_link_libraries(${EXE_TARGET}
  PRIVATE
    ${LIB_TARGET}
    Boost::program_options
)

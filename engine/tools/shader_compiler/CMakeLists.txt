cmake_minimum_required(VERSION 3.22)

set(TARGET "shader_compiler")

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(${TARGET} ${CPP_SOURCES})
target_link_libraries(${TARGET} PRIVATE ${ENGINE_LIB_TARGET} Vulkan::shaderc_combined)

target_include_directories(${TARGET} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

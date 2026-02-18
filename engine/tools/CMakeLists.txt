cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# shaderc lib provided by vulkan SDK isn't compatible with windows debug build
if(NOT (PLATFORM_WINDOWS AND CMAKE_BUILD_TYPE STREQUAL "Debug"))
  add_subdirectory(shader_compiler)
endif()
add_subdirectory(height_map_gen)
add_subdirectory(image_partitioner)
add_subdirectory(world_init)
#add_subdirectory(world_editor)

cmake_minimum_required(VERSION 3.22)

set(TARGET "editor")

find_package(wxWidgets CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"

  # TODO: Move to core
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/file_system.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/audio_system.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/stb_vorbis.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/linux/platform_paths.cpp"
)

add_executable(${TARGET} ${CPP_SOURCES})

target_include_directories(${TARGET} PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(${TARGET} PRIVATE ${GTK3_LIBRARY_DIRS})

target_link_libraries(${TARGET} PRIVATE
  ${ENGINE_LIB_TARGET}
  wx::base
  wx::gl
  ${GTK3_LIBRARIES}
)

target_include_directories(${TARGET} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:DEBUG>:${DEBUG_COMPILE_FLAGS}>")
target_compile_options(${TARGET} PRIVATE "$<$<CONFIG:RELEASE>:${RELEASE_COMPILE_FLAGS}>")

add_custom_target(
  link_editor_data ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data
)

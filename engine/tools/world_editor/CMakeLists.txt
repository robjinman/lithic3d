cmake_minimum_required(VERSION 3.22)

set(TARGET "editor")

find_package(wxWidgets CONFIG REQUIRED)

if(PLATFORM_LINUX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif()

file(GLOB CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"

  # TODO: Move to core
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/file_system.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/audio_system.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/default/stb_vorbis.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../src/linux/platform_paths.cpp"
)

if(PLATFORM_LINUX)
  file(GLOB PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux/*.cpp")
elseif(PLATFORM_WINDOWS)
  file(GLOB PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/*.cpp")
elseif(PLATFORM_OSX)
  file(GLOB PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/osx/*.cpp")
endif()

add_executable(${TARGET}
  ${CPP_SOURCES}
  ${PLATFORM_SOURCES}
)

if(PLATFORM_LINUX)
  target_include_directories(${TARGET} PRIVATE ${GTK3_INCLUDE_DIRS})
  target_link_directories(${TARGET} PRIVATE ${GTK3_LIBRARY_DIRS})
  # add_definitions(${GTK3_CFLAGS_OTHER})
elseif(PLATFORM_WINDOWS)
  target_link_options(${TARGET}
    PRIVATE
      /SUBSYSTEM:WINDOWS
      /ENTRY:WinMainCRTStartup
  )
elseif(PLATFORM_OSX)
  # TODO
endif()

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

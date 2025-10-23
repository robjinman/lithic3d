cmake_minimum_required(VERSION 3.22)

project("FreeholdGameEngine")

# TODO: Move this
if(ANDROID_BUILD)
  set(PLATFORM_ANDROID 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(PLATFORM_LINUX 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(PLATFORM_WINDOWS 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(PLATFORM_OSX 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
  set(PLATFORM_IOS 1)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PLATFORM_ANDROID)
  add_custom_target(android_bundle ALL
    COMMAND "${PROJECT_SOURCE_DIR}/platform/android/gradlew" "--info" "bundle${CMAKE_BUILD_TYPE}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/platform/android"
    USES_TERMINAL
    COMMENT "Running Gradle build..."
  )
else()
  add_subdirectory("core" "core")
  add_subdirectory("${GAME_PROJECT_DIRECTORY}" "game")

  set(EXE_TARGET "${GAME_SHORT_NAME}")

  if(PLATFORM_LINUX)
    find_package(glfw3 REQUIRED)
    add_executable(${EXE_TARGET}
      "${PROJECT_SOURCE_DIR}/src/default/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/linux/platform_paths.cpp"
      "${PROJECT_SOURCE_DIR}/core/src/vulkan/platform/default/vulkan_window_delegate.cpp" # TODO: Move to core target?
    )
    target_link_libraries(${EXE_TARGET} PRIVATE
      glfw
    )
    if (CPU_PROFILE)
      target_link_libraries(${EXE_TARGET} PRIVATE
        -Wl,--no-as-needed -lprofiler -Wl,--as-needed
      )
    endif()
  elseif(PLATFORM_WINDOWS)
    find_package(glfw3 REQUIRED)
    add_executable(${EXE_TARGET}
      "${PROJECT_SOURCE_DIR}/src/default/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/windows/platform_paths.cpp"
      "${PROJECT_SOURCE_DIR}/core/src/vulkan/platform/default/vulkan_window_delegate.cpp"
    )
    set_target_properties(${EXE_TARGET} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
    target_link_libraries(${EXE_TARGET} PRIVATE
      glfw
      winmm
    )
  elseif(PLATFORM_OSX)
    find_package(glfw3 REQUIRED)
    add_executable(${EXE_TARGET}
      "${PROJECT_SOURCE_DIR}/src/default/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/linux/platform_paths.cpp"
      "${PROJECT_SOURCE_DIR}/core/src/vulkan/platform/default/vulkan_window_delegate.cpp"
    )
    set_target_properties(${EXE_TARGET} PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "${GAME_FULL_NAME}"
      MACOSX_BUNDLE_BUNDLE_NAME "${GAME_FULL_NAME}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_STRING}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_STRING}"
      MACOSX_BUNDLE_ICON_FILE "app.icns"
    )
    target_link_libraries(${EXE_TARGET} PRIVATE
      glfw
      "-framework OpenAL"
    )
  elseif(PLATFORM_IOS) 
    set(LAUNCH_STORYBOARD ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/LaunchScreen.storyboard)
    set(LAUNCH_STORYBOARDC ${CMAKE_CURRENT_BINARY_DIR}/LaunchScreen.storyboardc)

    add_custom_command(
      OUTPUT ${LAUNCH_STORYBOARDC}
      COMMAND ibtool --errors --warnings --notices --output-format human-readable-text
        --compile ${LAUNCH_STORYBOARDC} ${LAUNCH_STORYBOARD}
      DEPENDS ${LAUNCH_STORYBOARD}
      COMMENT "Compiling LaunchScreen.storyboard"
    )

    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/platform/ios/cmake")
    set(MACOSX_BUNDLE_BUNDLE_NAME "${GAME_FULL_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_STRING}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "%{GAME_BUNDLE_IDENTIFIER}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${GAME_VERSION_STRING}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_STRING}")
    set(AUDIO_SYSTEM_CPP "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp")
    # Uncomment for iOS simulator
    #set(AUDIO_SYSTEM_CPP "${PROJECT_SOURCE_DIR}/src/ios_sim/audio_system.cpp")
    add_executable(${EXE_TARGET} MACOSX_BUNDLE
      "${PROJECT_SOURCE_DIR}/src/ios/wrapper.mm"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/ios/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/ios/platform_paths.cpp"
      "${AUDIO_SYSTEM_CPP}"
      "${PROJECT_SOURCE_DIR}/core/src/vulkan/platform/ios/vulkan_window_delegate.mm"
      "${GAME_PROJECT_DIRECTORY}/data"
      "$ENV{VULKAN_SDK}/../iOS/share/vulkan"
      "${LAUNCH_STORYBOARDC}"
      "${PROJECT_SOURCE_DIR}/platform/ios/Assets.xcassets"
    )
    set_source_files_properties(
      "$ENV{VULKAN_SDK}/../iOS/share/vulkan"
      "${GAME_PROJECT_DIRECTORY}/data"
      "${LAUNCH_STORYBOARDC}"
      "${PROJECT_SOURCE_DIR}/platform/ios/Assets.xcassets"
      PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_link_libraries(${EXE_TARGET} PRIVATE
      "-framework UIKit"
      "-framework Foundation"
      "-framework CoreGraphics"
      "-framework Metal"
      "-framework QuartzCore"
      "-framework IOKit"
      "-framework IOSurface"
      "-framework GameController"
    )
    set(CMAKE_OSX_DEPLOYMENT_TARGET "17")
    set(CMAKE_XCODE_GENERATE_SCHEME "TRUE")
    set(CMAKE_XCODE_SCHEME_ENABLE_GPU_API_VALIDATION "FALSE")
    set(CMAKE_XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "DISABLED")
    set_target_properties(${EXE_TARGET} PROPERTIES
      BUNDLE_IDENTIFIER "${GAME_BUNDLE_IDENTIFIER}"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${GAME_BUNDLE_IDENTIFIER}"
      XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "YES"
      XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES "YES"
      XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
      XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_STRING}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_STRING}"
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "LW57Q4U5U7"
      XCODE_EMBED_FRAMEWORKS "${Vulkan_MoltenVK_LIBRARY};${Vulkan_LIBRARY}" #${Vulkan_Layer_VALIDATION}
      XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY "YES"
      XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY "YES"
      XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
      XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
      XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
      XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING "NO"
    )
  endif()

  target_link_libraries(${EXE_TARGET} PRIVATE libgame)

  add_custom_target(
    link_data ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${GAME_PROJECT_DIRECTORY}/data ${PROJECT_BINARY_DIR}/data
  )

  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
  include(InstallRequiredSystemLibraries)

  if(PLATFORM_OSX)
    install(TARGETS ${EXE_TARGET}
      BUNDLE DESTINATION .
      RUNTIME DESTINATION .
    )
    install(DIRECTORY "${GAME_PROJECT_DIRECTORY}/data"
      DESTINATION "${EXE_TARGET}.app/Contents/Resources")
    install(FILES "${PROJECT_SOURCE_DIR}/platform/osx/app.icns"
      DESTINATION "${EXE_TARGET}.app/Contents/Resources")
    install(FILES "/usr/local/lib/libMoltenVK.dylib"
      DESTINATION "${EXE_TARGET}.app/Contents/Frameworks")
    install(FILES "${PROJECT_SOURCE_DIR}/platform/osx/MoltenVK_icd.json"
      DESTINATION "${EXE_TARGET}.app/Contents/Resources/vulkan/icd.d")

    install(CODE "
      include(BundleUtilities)
      fixup_bundle(\"${CMAKE_INSTALL_PREFIX}/${EXE_TARGET}.app\" \"${CMAKE_INSTALL_PREFIX}/${EXE_TARGET}.app/Contents/Frameworks/libMoltenVK.dylib\" \"\")
      " COMPONENT Runtime)

    install(CODE "execute_process(COMMAND codesign --deep --force --sign - ${CMAKE_INSTALL_PREFIX}/${EXE_TARGET}.app)")
  else()
    install(TARGETS ${EXE_TARGET} DESTINATION ".")
    install(DIRECTORY "${GAME_PROJECT_DIRECTORY}/data" DESTINATION ".")
  endif()

  set(CPACK_PACKAGE_VENDOR "Freehold Apps Ltd")
  set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH 0)
  include(CPack)

  if(PLATFORM_LINUX OR PLATFORM_OSX OR PLATFORM_WINDOWS)
    add_subdirectory("tools")
    add_subdirectory("test")
  endif()

endif()

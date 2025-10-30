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

if(DEFINED IOS_SIMULATOR)
  message("Configuring project for iOS Simulator")
endif()

if(PLATFORM_ANDROID)
  add_custom_target(
    link_data_to_assets ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${GAME_PROJECT_DIRECTORY}/data "${PROJECT_SOURCE_DIR}/platform/android/data/src/main/assets"
  )

  add_custom_target(
    link_icons_android ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${GAME_PROJECT_DIRECTORY}/icons/android "${PROJECT_SOURCE_DIR}/src/android/src/main/res"
  )

  add_custom_target(android_bundle ALL
    COMMAND "${PROJECT_SOURCE_DIR}/platform/android/gradlew"
      "--info" "bundle${CMAKE_BUILD_TYPE}"
      "-PgameShortName=${GAME_SHORT_NAME}"
      "-PgameBundleId=${GAME_BUNDLE_IDENTIFIER}"
      "-PgameProjectDir=${GAME_PROJECT_DIRECTORY}"
      "-PgameVersionMajor=${GAME_VERSION_MAJOR}"
      "-PgameVersionMinor=${GAME_VERSION_MINOR}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/platform/android"
    USES_TERMINAL
    COMMENT "Running Gradle build..."
  )
else()
  add_subdirectory("core")
  add_subdirectory("${GAME_PROJECT_DIRECTORY}" "${GAME_SHORT_NAME}")

  set(EXE_TARGET "${GAME_SHORT_NAME}")

  if(PLATFORM_LINUX)
    find_package(glfw3 REQUIRED)
    add_executable(${EXE_TARGET}
      "${PROJECT_SOURCE_DIR}/src/default/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/stb_vorbis.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/vulkan_window_delegate.cpp"
      "${PROJECT_SOURCE_DIR}/src/linux/platform_paths.cpp"
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
      "${PROJECT_SOURCE_DIR}/src/default/stb_vorbis.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/vulkan_window_delegate.cpp"
      "${PROJECT_SOURCE_DIR}/src/windows/platform_paths.cpp"
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
      "${PROJECT_SOURCE_DIR}/src/default/stb_vorbis.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/product_activation.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/drm.cpp"
      "${PROJECT_SOURCE_DIR}/src/default/vulkan_window_delegate.cpp"
      "${PROJECT_SOURCE_DIR}/src/linux/platform_paths.cpp"
    )
    set_target_properties(${EXE_TARGET} PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_GUI_IDENTIFIER "${GAME_DISPLAY_NAME}"
      MACOSX_BUNDLE_BUNDLE_NAME "${GAME_DISPLAY_NAME}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      MACOSX_BUNDLE_ICON_FILE "app.icns"

      # TODO: Remove info.plist file?
#      XCODE_ATTRIBUTE_PRODUCT_NAME              "MyApp"
#      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.example.myapp"
#      XCODE_ATTRIBUTE_MARKETING_VERSION         "${PROJECT_VERSION}"
#      XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION   "${BUILD_VERSION}"
#      XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE   YES
#      XCODE_ATTRIBUTE_INFOPLIST_FILE            ""
#      XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName       "My Cool Game"
#      XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
#      XCODE_ATTRIBUTE_INFOPLIST_KEY_NSPrincipalClass          "NSApplication"
#      XCODE_ATTRIBUTE_INFOPLIST_KEY_NSMainStoryboardFile      "Main"
#      XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright  "(c)2023 Example Co."
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

    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist"
      "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )

    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/platform/ios/cmake")
    set(MACOSX_BUNDLE_BUNDLE_NAME "${GAME_DISPLAY_NAME}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "%{GAME_BUNDLE_IDENTIFIER}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}")
    set(AUDIO_SYSTEM_CPP "${PROJECT_SOURCE_DIR}/src/default/audio_system.cpp")
    if(DEFINED IOS_SIMULATOR)
      set(AUDIO_SYSTEM_CPP "${PROJECT_SOURCE_DIR}/src/ios_sim/audio_system.cpp")
    endif()
    add_executable(${EXE_TARGET} MACOSX_BUNDLE
      "${PROJECT_SOURCE_DIR}/src/ios/wrapper.mm"
      "${PROJECT_SOURCE_DIR}/src/default/file_system.cpp"
      "${PROJECT_SOURCE_DIR}/src/ios/application.cpp"
      "${PROJECT_SOURCE_DIR}/src/ios/platform_paths.cpp"
      "${PROJECT_SOURCE_DIR}/src/ios/vulkan_window_delegate.mm"
      "${AUDIO_SYSTEM_CPP}"
      "${PROJECT_SOURCE_DIR}/src/default/stb_vorbis.cpp"
      "${GAME_PROJECT_DIRECTORY}/data"
      "$ENV{VULKAN_SDK}/../iOS/share/vulkan"
      "${LAUNCH_STORYBOARDC}"
      "${GAME_PROJECT_DIRECTORY}/icons/ios/Assets.xcassets"
    )
    set_source_files_properties(
      "$ENV{VULKAN_SDK}/../iOS/share/vulkan"
      "${GAME_PROJECT_DIRECTORY}/data"
      "${LAUNCH_STORYBOARDC}"
      "${GAME_PROJECT_DIRECTORY}/icons/ios/Assets.xcassets"
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
    #set(CMAKE_OSX_DEPLOYMENT_TARGET "17")
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
      #XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH "NO" # Comment out for simulator
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${GAME_APPLE_TEAM_ID}"
      XCODE_EMBED_FRAMEWORKS "${Vulkan_MoltenVK_LIBRARY};${Vulkan_LIBRARY}" #${Vulkan_Layer_VALIDATION}
      XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY "YES"
      XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY "YES"
      XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
      XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
      XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
      XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING "NO"
      XCODE_ATTRIBUTE_PRODUCT_NAME "${GAME_SHORT_NAME}"
      XCODE_ATTRIBUTE_MARKETING_VERSION "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${GAME_VERSION_MAJOR}.${GAME_VERSION_MINOR}"
      XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE "NO"
      XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
      #XCODE_ATTRIBUTE_INFOPLIST_FILE "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist"
      #XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName "${GAME_DISPLAY_NAME}"
      #XCODE_ATTRIBUTE_INFOPLIST_KEY_LSApplicationCategoryType "public.app-category.games"
      #XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright "${GAME_COPYRIGHT_STRING}"
      #XCODE_ATTRIBUTE_INFOPLIST_KEY_UIDeviceFamily "1,2"
    )
  endif()

  if(NOT PLATFORM_IOS) # TODO: Need this for iOS?
    set_target_properties(${EXE_TARGET}
      PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${GAME_SHORT_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${GAME_SHORT_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${GAME_SHORT_NAME}"
    )
  endif()

  target_link_libraries(${EXE_TARGET} PRIVATE game)

  add_custom_target(
    link_data ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${GAME_PROJECT_DIRECTORY}/data ${PROJECT_BINARY_DIR}/${GAME_SHORT_NAME}/data
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

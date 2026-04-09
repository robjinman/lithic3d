Android Debugging
=================

Lithic3D's build system is CMake, which provides a unified way of building the engine (and application) across a wide range of platforms. With the use of CMake's presets feature, the build command takes the form

```
    LITHIC3D_PROJECT=../your_app_directory cmake --workflow --preset=platform-config
```

Achieving this was not easy. It was especially challenging for Android builds (which run from a Linux host).

Typical (non-android) build sequence
------------------------------------

For non-android builds, the sequence is as follows:

1. The developer runs CMake from the project root, which runs the top level CMakeLists.txt using variables defined in CMakePresets.json. The relevant toolchain for the platform is provided by VCPKG.
2. This top-level CMakeLists file reads the variables from the application's game.cmake file and then invokes engine/CMakeLists.txt.
3. engine/CMakeLists.txt builds the following: (a) the platform independent engine runtime (core), (b) the platform-specific sources, (c) the application code, (d) the unit tests, (e) and the tools. It does this by, respectively: invoking engine/core/CMakeLists.txt (a), compiling the relevant sources under engine/src/platform (b), invoking the CMakeLists file in the application directory (c), invoking engine/test/CMakeLists.txt (d), and invoking engine/tools/CMakeLists.txt (e). It then links the engine runtime (a) with platform-specific code (which contains the program's entry point) (b) and the application static library (c) into a final executable.

Android build sequence
----------------------

1. As before, the developer runs CMake from the project root, which runs the top level CMakeLists.txt using variables defined in CMakePresets.json. The relevant toolchain for the platform is provided by VCPKG.
2. As before, this top-level CMakeLists file reads the variables from the application's game.cmake file and then invokes engine/CMakeLists.txt.
3. engine/CMakeLists.txt kicks off the gradle build in directory engine/platform/android, invoking the bundleCONFIG target, where CONFIG is one of Release or Debug.
4. engine/platform/android/settings.gradle locates the application's data directory and invokes engine/src/android/build.gradle.
5. engine/src/android/build.gradle kicks off a CMake build of engine/core.
6. engine/core/CMakeLists.txt, rather than just building core, builds core plus the android sources under engine/src/android and the application sources.

The result is a .aab file located at build/android/gradle_output/APP_NAME/outputs/bundle/CONFIG, where APP_NAME is the name of the application and CONFIG is one of 'release' or 'debug'.

Perhaps the worst part of this is engine/core/CMakeLists.txt doing very different things for non-android versus android builds. In the former case, it builds just the core library. On Android, it acts more like the higher level engine/CMakeLists.txt and builds core, the application sources, and the platform-specific sources. This is confusing and could almost certainly be improved.

Debugging
---------

### What doesn't work

Ideally, it would be possible to debug the Android bundle using only command-line tools. This would go something like this:

```
    adb shell mkdir -p /data/local/tmp/tools
    adb push ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/lib/clang/18/lib/linux/aarch64/* /data/local/tmp/tools
    adb forward tcp:5039 tcp:5039
    adb shell /data/local/tmp/tools/lldb-server platform --listen '*:5039' --server
```

In a separate shell

```
    adb shell am start -D -n com.example.myapp/android.app.NativeActivity
    PID=$(adb shell pidof com.example.myapp)

    lldb -o "platform select remote-android" \
        -o "platform connect connect://:5039" \
        -o "process attach --pid ${PID}"
```

Unfortunately, I couldn't get this to work. I suspect I was running into permission problems. On a rooted device it might be possible.

### What does work

It's possible to debug the app with Android Studio, but it's not at all obvious how to do it.

This method has been tested with Android Studio Panda 3.

1. Generate a debug build and an output.apks file as described in the main README.
2. Install the app as usual.
3. Launch the app. You can have it pause on startup using adb: `adb shell am start -D -n com.example.myapp/android.app.NativeActivity`. You should see 'Waiting For Debugger' on the device.
4. Open output.apks and extract the APK with the architecture matching your target device, e.g. splits/base-arm64_v8a.apk.
5. Run Android Studio and create or open any project.
6. Select File -> 'Profile or Debug APK', and select the APK file.
7. You'll see a banner near the top of the window warning about missing debug symbols. Load the symbols from /build/android/gradle_output/APP_NAME/intermediates/cxx/Debug/XXXXXXXX/obj/arm64-v8a/libapp.so, replacing APP_NAME and XXXXXXXX as appropriate.
8. Go to Run -> 'Attach Debugger to Android Process' and where it says 'Use Android Debugger Settings from' select the APK name from the drop-down.

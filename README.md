Lithic3D
========

C++/Vulkan game engine supporting Windows, Linux, Mac, iPhone, and Android.

Building from source
--------------------

You build the engine and your game at the same time with a single build command that will look something like:

```
    LITHIC3D_PROJECT=../game cmake --workflow --preset=linux-debug
```

where ../game is the path to your game project. To start a new project, copy and modify one of the examples.

### Prerequisites

#### Linux

* cmake
* vcpkg
* Vulkan SDK

#### OS X and iOS

* XCode
* cmake
* vcpkg
* Vulkan SDK
* homebrew

#### Windows

* Visual Studio 17 2022
* cmake
* vcpkg
* Vulkan SDK

Make sure everything you need is on your PATH, including objdump.exe, which should be installed with Visual Studio. You can find the location of objdump by opening a Native Tools command prompt and typing `where objdump`.

#### Android

Linux host with the same prerequisites as above, plus

* Android Studio
* Android NDK (can be installed through Android Studio)

Make sure environment variables ANDROID_HOME and ANDROID_NDK_HOME are set.

### Build

To build, just run the relevant workflow from the project root, specifying your project location in the environment variable LITHIC3D_PROJECT.

To see the list of workflows

```
    cmake --workflow --list-presets
```

For example, to make a debug build on linux

```
    LITHIC3D_PROJECT=../game cmake --workflow --preset=linux-debug
```

You can also run the configure/build steps separately

```
    cmake --preset=linux-debug
    cmake --build --preset=linux-debug
```

#### Android

The build output is an AAB bundle located under build/android/gradle_output/outputs/bundle, which can be installed using the [bundle tool](https://github.com/google/bundletool/releases).

Convert the bundle to APKs

```
    java -jar ~/Downloads/bundletool-all-1.18.1.jar build-apks --bundle=./app-debug.aab --output=output.apks --local-testing
```

Install the APKs

```
    java -jar ~/Downloads/bundletool-all-1.18.1.jar install-apks --apks=output.apks
```

Before uploading to the Play Store, sign the bundle

```
    jarsigner -verbose -sigalg SHA256withRSA -digestalg SHA-256 -keystore ~/path/to/upload-keystore.jks app-release.aab upload
```

To generate the keys, run

```
    keytool -genkeypair -v -keystore upload-keystore.jks -alias upload -keyalg RSA -keysize 4096 -validity 20000
```

#### iOS

If Vulkan isn't found, source the setup-env.sh file before building for iOS

```
    . ~/VulkanSDK/1.3.280.1/iOS/setup-env.sh
```

To install on the device

```
    xcrun devicectl device install app --device <id> ./build/ios/debug/Debug-iphoneos/game.app
```

You can obtain the device ID with

```
    xcrun devicectl list devices
```

To publish to the App Store

```
    xcodebuild -project ./build/ios/release/engine/Lithic3D.xcodeproj -scheme ALL_BUILD -configuration Release -sdk iphoneos -archivePath ./build/ios/game.xcarchive clean archive

    xcodebuild -exportArchive -archivePath ./build/ios/game.xcarchive -exportPath ./build/ios/export -exportOptionsPlist ./engine/platform/ios/ExportOptions.plist -allowProvisioningUpdates
```

Then use the Transporter app to perform the upload.

### Creating deployables

#### OS X

Build icon set for OSX

```
    brew install imagemagick

    cd ./engine/platform/osx
    ./build_icon_set ./path/to/icon.png ./destination/directory
```

You should put the icon set in $LITHIC3D_PROJECT/icons/osx.

After running the osx-release preset, create an .app bundle with

```
    cmake --install ./build/osx/release
```

#### Windows and Linux

Zip bundles are created by the release presets.

### Example projects

A number of example projects are provided that demonstrate how to use various features. The easiest way to start a new Lithic3D project is to copy and modify one of them.

To build one of the example projects such as 1_cube on linux

```
    LITHIC3D_PROJECT=./examples/1_cube cmake --workflow --preset=linux-debug
```

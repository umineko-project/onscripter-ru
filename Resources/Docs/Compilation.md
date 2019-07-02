ONScripter-RU Compilation
=========================

This document does not cover all the compilation steps for every platform, but tries to provide a general idea to give one the right direction on the minimal recommended platform versions as follows:

- macOS 10.6 (i386 SSE3, x86_64)
- macOS 10.7 (x86_64h)
- iOS 8.0 (armv7, armv7s, aarch64)
- Debian Linux 9 (x86_64)
- Ubuntu Linux 18.04 (x86_64)
- Windows XP SP3 (i686 SSE3)
- Android 4.1 (i386 SSE3, armv7, aarch64)

Build stacks use a shared dependency compilation system called **onscrlib** (see `Dependencies` folder for more details). The supported build stacks are as follows:

- Xcode (macOS and iOS support)
- configure & make (everything else)

A generic way to compile the project for the host is as follows:

```
cd /path/to/onscripter
./configure # Add --release-build --strip-binary for release builds
make -j8    # Add DEBUG=1 for debugging
```

#### macOS and iOS

[Xcode](https://developer.apple.com/xcode/) is a requirement regardless of the compilation method. Xcode 9.2 or 9.4 is a recommended choice, Xcode 10 is **NOT** recommended, as you may not be able to target 32-bit 10.6, and its new build system may not be optimised for ONScripter-RU needs. It is suggested to use [MacPorts](https://www.macports.org), as it is supported by Apple and can provide the necessary tools at easy cost.

1. Install the dependencies required to build onscrlib.
```
sudo port install automake autoconf yasm pkgconfig gmake cmake
```
2. Install custom clang compiler if you plan to target 10.6.  
At least Clang 5.0 is required, any newer should work too. You can use the one from MacPorts by running:
```
sudo port install llvm-7.0 clang-7.0 cctools +xcode ld64 +ld64_xcode
```
To enable external clang support in Xcode perform the following steps:
    1. Recursively create ~/Library/Application Support/Developer/Shared/Xcode/Plug-Ins directory.
    2. Copy a provided xcplugin to it.
    3. Add your Xcode DVTPlugInCompatibilityUUID to the plugin:
```
XCODEUUID=$(defaults read /Applications/Xcode.app/Contents/Info DVTPlugInCompatibilityUUID)
defaults write ~/Library/Application\ Support/Developer/Shared/Xcode/Plug-Ins/Clang\ MacPorts.xcplugin/Contents/Info.plist DVTPlugInCompatibilityUUIDs -array-add $XCODEUUID
```
    4. Patch `/opt/local/libexec/llvm-7.0/include/c++/v1/__config` to fix libcxxabi compilation by replacing:
```
	#define _LIBCPP_AVAILABILITY_TYPEINFO_VTABLE                                   \
	  __attribute__((availability(macosx,strict,introduced=10.9)))                 \
	  __attribute__((availability(ios,strict,introduced=7.0)))
```
with:
```
	#define _LIBCPP_AVAILABILITY_TYPEINFO_VTABLE                                   \
	  __attribute__((availability(macosx,strict,introduced=10.6)))                 \
	  __attribute__((availability(ios,strict,introduced=7.0)))
```
3. Run Xcode and select the project of your choice:
    - `onscripter-ru-osx64h` for native compilation for macOS 10.7+
    - `onscripter-ru-ios` for native compilation for iOS 8.0+
    - `onscripter-ru-osx64` for custom compilation for macOS 10.6+ x86_64
    - `onscripter-ru-osx32` for custom compilation for macOS 10.6+ i386
    - `onscripter-ru-osx` to create an aggregated FAT binary with all osx targets
4. Set the compiler to `Clang MacPorts` and update `Clang compiler path` if you use custom compilation targets in project settings.
5. Set custom working directory in `Edit Scheme` → `Options`.
6. Set build configuration (`Release` or `Debug`) in `Edit Scheme` → `Options`.
7. Build and debug.


**NOTES**:

- It is recommended to use project-relative path to DerivedData in Xcode preferences, as some build scripts assume it by default.
- You can obviously attempt to use command line compilation. Follow Linux recommendations after installing the dependencies. This is not a supported option and has limitations with Cocoa integration.
- `onscripter-ru-osx64h` has AVX 2 extensions turned on, make sure to turn them off if your CPU is older than Haswell.
- For iOS ipa generation use `Scripts/ipabuild.tool` after compiling the app in Xcode.

#### Cross-compiling for Windows

Cross compiling is the easiest way to get Windows binaries.

1. Install the MinGW-W64 dependencies for i686. On macOS this could be done with a MacPorts command:
```
sudo port install i686-w64-mingw32-binutils i686-w64-mingw32-crt i686-w64-mingw32-gcc i686-w64-mingw32-headers
```
2. Run the necessary commands:
```
cd /path/to/onscripter
export CC=i686-w64-mingw32-gcc
export CXX=i686-w64-mingw32-g++
export LD=i686-w64-mingw32-ld
export AR=i686-w64-mingw32-ar
export RANLIB=i686-w64-mingw32-ranlib
export AS=i686-w64-mingw32-as
chmod a+x configure
./configure --cross=i686-w64-mingw32
make
```

#### Host-compiling Windows

Windows compilation is normally the most difficult one due to Linux build tools ported not ideally to a Microsoft system.

You will need these tools:

* [MSYS2](https://msys2.github.io/) (pick an installer file according to your system architecture)
* [CLion](https://www.jetbrains.com/clion/) or [CodeLite](http://codelite.org/) for a more convenient debugging interface (optional)

1. Install MSYS2 to `C:\msys64` (installing to other locations and using CLion require one to change `MSYS_PATH` in CMakeLists.txt).
2. Update MSYS2 core (always use `mingw32.exe`):
```
pacman -Syu
```
3. Close MSYS2 at that point and run the following command after reopening it:
```
pacman -Syu
```
4. Repeat the previous action until you are fully updated.
5. Install the required packages via pacman:
```
pacman -S base-devel git mercurial subversion unzip yasm mingw-w64-i686-toolchain mingw-w64-i686-cmake python
```
6. Optionally install these packages:
```
pacman -S mingw-w64-i686-codelite-git mingw-w64-i686-gcc-debug mingw-w64-i686-clang
```
7. Proceed using the generic method of compilation at the beginning of these instructions. Provide `--prefer-clang` configure argument if using Clang.

**NOTES**:

- GDB may find no source in your executable, `make DEBUG=1` is needed to build a debug binary.
- If you need to build a shared SDL2 library, after you change `--disable-shared` to `--disable-static` you may get an error on compilation step with `SDL_window_main.o` not found. To fix that you are in need to go to SDL2 sources and copy the contents of build/.libs to build (perhaps one more time after next step). Then manually run `make` and `make install`. To mark the package as built run `touch onscrlib/onscrlib/.pkgs/SDL2`.
- Latest gdb versions from MSYS2 distribution do not always work properly in Codelite. A slightly older mingw build may be more stable (try [gdb2014-05-23.zip](https://sourceforge.net/projects/gdbmingw/files/)).
- You may run into issues if you forget to start MSYS2 via `mingw32.exe`.
- You must remember that MSYS2 uses linux-style slashes for paths. This means a path `C:\Directory\AnotherDir` should be written as `/c/Directory/AnotherDir` in MSYS2.
- First compilation must be performed outside of CLion due to several incompatibilities.
- Using `make -j4` or similar is prohibited for the first compilation and is not recommended when building with gcc due to MinGW issues.

**Using CLion**:

As an alternative to Codelite you may use CLion IDE created by JetBrains. Copy the onscripter/.idea folder inside your onscripter directory with configured target and open the project from CLion IDE. Most of the actions can be found by pressing Ctrl+Shift+A combination and typing them. A short problem list includes:

- Uneasy navigation (Use favourites window with project viewer in file structure mode)
- Slow step-by-step debugging (decrease Value tooltip display in Debug settings)
- Missing class variables when debugging (disable Hide out-of-scope variables option)
- Annoying typo finds (disable spelling correction in settings)
- No line numbers ("Editor" → "Appearance" → "Enable line numbers")
- Spaces instead of TABs (enable Use TAB character and disable Detect and use existing file indents for editing)

#### Android

**Prerequisities**:

- everything necessary to build a hosted engine
- openssl command line tool
- zip command line tool (pacman -S zip in msys2)
- wget or curl command line tool
- libtool for libunwind compilation

**Basic compilation guide**:

This guide is useful for development when targeting a single device with a single architecture:

1. Run configure for your target architecture:
```
./configure --droid-build --droid-arch=arm
```
The target architecture is one of arm, arm64, and x86. Please note, that the configure script will download and setup the ndk for any architecure if necessary on every run. All the normal configure options from the beginning of the document apply.
2. Make the engine:
```
make -j8
```
3. Create the apk and grab it from the Droid-package subfolder in the build directory:
```
make apk
```

**Multiple architecture compilation guide**:

To compile for multiple architectures (i.e. create a FAT apk file) for deployment you could either use `./Scripts/quickdroid.tool` tool or run the following commands manually:
```
./configure --droid-build --droid-arch=arm
make
./configure --droid-build --droid-arch=arm64
make
./configure --droid-build --droid-arch=x86
make
make apkall
```

`./Scripts/quickdroid.tool` accepts the following arguments:
- `--normal` — normal developer build (default)
- `--release` — stripped release build
- `--debug` — debug build

**Debugging the binaries**:

It is recommended to debug using IDA Pro.

1. Setting Java debugger in order to properly start the application. It is worth checking the [official documentation](https://www.hex-rays.com/products/ida/support/tutorials/debugging_dalvik.pdf) first.
   
    1. Open classes.dex in (32-bit) IDA Pro by dragging onscripter-ru.apk into its main window
    2. Put a breakpoint on `_def_Activity__init_@V`
    3. Go to `Debugger` → `Debugger options` → Set specific options and fill adb path
    4. Launch the debugger and specify source path mapping (`.` → `path/to/onscripter/sources`)

2. Setting hardware debugger in order to debug the binary.

    1. Open `libmain.so` in IDA Pro by dragging `onscripter-ru.apk` into its main window
    2. Set debugger to `Remote Linux Debugger`
    3. Upload a correct android debugger server to the device (e.g. to `/data/debug/`):
        - `android_server` — for arm
        - `android_server64` — for arm64
        - `android_x86_server` — for x86

        You may use the following command:
        ```
        adb push android_server /data/debug/
        ```

    4. Set debugger executable permissions to 0777 and run the debugger (use adb shell).
    5. Set `Debugger` → `Process` options parameters:
        - Application and Input file to your device libmain.so path, e.g.:
            ```
            /data/app-lib/org.umineko_project.onscripter_ru-1/libmain.so
            ```
        - Hostname to your device IP address, e.g.:
            ```
            192.168.1.111
            ```
        - Directory to your src directory, e.g.:
            ```
            path/to/onscripter/sources
            ```
    6. Ignore any warnings.
    7. Add a breakpoint to `SDL_main`

3. Using the debugger.

    1. Start the process in IDA Java instance
    2. Attach to the process in IDA Native instance
    3. Detach from the process in IDA Java instance (or just ignore it)
    4. Enjoy

**NOTES**:

- No Java installation, Android SDK, or tools are necessary to build onscripter-ru
- Only arm-v7a, aarch64, and x86 binaries are compiled
- Building on Linux and Windows systems is mostly untested
- Building standalone onscrlib package may fail on Windows due to `%PATH%`/`$PATH` design
- Source level debugging may not always be available
- The logs are generated with ONScripter-RU and SDL tags:
```
adb logcat | grep -E '(ONScripter-RU|SDL)'
```

#### Building Android Java sources

Even though all the Java-dependent files are provided in compiled form you may rebuild them.

1. Download [Java SE Development Kit](http://www.oracle.com/technetwork/java/javase/downloads/jdk8-downloads-2133151.html) (not Java RE) for your platform.
2. Download [Android command line tools](https://developer.android.com/studio/index.html#downloads) for your platform (avoid Android Studio itself).
3. Extract the downloaded tools some folder e.g. `$HOME/droid/tools`
4. Install the following packages:
    - `tools` (Android SDK Tools)
    - `platform-tools` (Android SDK Platform-tools)
    - `build-tools;26.0.0` (Android SDK Build-tools)
    - `platforms;android-26` (Android 8.0 (API 26) → SDK Platform (others are not tested))

    On Windows run android.bat and manually uncheck everything else.  
    On Other platforms you could run the following command:
    ```
    ./bin/sdkmanager tools platform-tools 'platforms;android-26' 'build-tools;26.0.0'
    ```

5. Recompile the resources by running the following command:
    ```
    ./Scripts/apkbuild.sh DerivedData --recompile
    ```

    The following arguments are supported:

    - `--recompile` — rebuilds Java/Android sources
    - `--jsign` — signs apk file with jarsigner

    The following environment variables are supported:

    - `JAVA_PATH` — path to `bin/javac`
    - `DROID_TOOLS` — path to `aapt` tool
    - `DROID_PLATFORM` — path to `android.jar`

You will have to copy the recompiled binaries from `DerivedData/Droid-package/bin` to `Resources/Droid/bin` for later usage without the need to recompile and building by running `make apk` or `make apkall`.

#### Linux Ubuntu 18.04/Debian 9/SteamOS

1. You will need a number of packages:
```
apt-get install build-essential cmake subversion mercurial git libreadline-dev autoconf yasm libasound-dev libgl1-mesa-dev libegl1-mesa-dev libxrandr-dev pkg-config unzip
```
    1. You may not need mesa GL packages if you use NVIDIA proprietary drivers.
2. Don't forget to set an executive bit for shell files, otherwise it may fail:
```
chmod +x configure Scripts/* Dependencies/build.sh
```
3. Proceed using the generic method of compilation at the beginning of these instructions.

_Unlike macOS (manual architecture specification) and Windows (untested 64-bit binary), the executable architecture on Linux depends on the default compiler architecture. On 32-bit systems 32-bit binaries are normally produced and on 64-bit systems — 64-bit binaries._

#### Linux OpenSUSE

1. You will need a number of packages (similarly to Debian). Install them with YaST or whatever you like:
```
cmake, autoconf, automake, yasm, mercurial, subversion, git, libtool, gcc-c++, patch, readline-devel, Mesa-libGL-devel, freeglut-devel, glibc-devel-static, alsa-devel
```
2. Don't forget to set an executive bit for shell files, otherwise it may fail:
```
chmod +x configure Scripts/* Dependencies/build.sh
```
3. Proceed using the generic method of compilation at the beginning of these instructions.

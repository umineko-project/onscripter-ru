#!/bin/bash

#
# apkbuild.tool
# ONScripter-RU
#
# Droid apk generation script.
# Run with "build/dir" [--jsign] [--recompile] arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

pushd "$(dirname "$0")" &>/dev/null
SCRIPTS="$(pwd)"
popd &>/dev/null

if [ "$1" == "" ]; then
  WORK="$SCRIPTS/../DerivedData"
else
  WORK="$1"
fi

RECOMPILE=false
JAVASIGN=false

if [ "$2" == "--recompile" ] || [ "$3" == "--recompile" ]; then
  RECOMPILE=true
fi
if [ "$2" == "--jsign" ] || [ "$3" == "--jsign" ]; then
  JAVASIGN=true
fi

pushd "$WORK" &>/dev/null
WORK="$(pwd)"
popd &>/dev/null

if [ ! -d "$WORK" ] || [ ! -d "$SCRIPTS/../Resources/Droid" ]; then
  echo "Invalid launch directory!"
  exit 1
fi

PKGPATH="$WORK/Droid-package"
SDLACTPATH="$PKGPATH/src/SDLActivity.java"
ONSACTPATH="$PKGPATH/src/ONSActivity.java"
SDLJPATH="$PKGPATH/src/SDL.java"
SDLAUDIOPATH="$PKGPATH/src/SDLAudioManager.java"
SDLCTRLPATH="$PKGPATH/src/SDLControllerManager.java"
HIDDEVPATH="$PKGPATH/src/HIDDevice.java"
HIDBLEPATH="$PKGPATH/src/HIDDeviceBLESteamController.java"
HIDMGRPATH="$PKGPATH/src/HIDDeviceManager.java"
HIDUSBPATH="$PKGPATH/src/HIDDeviceUSB.java"
LIBPATH="$PKGPATH/lib"
BINPATH="$PKGPATH/bin"
RESPATH="$PKGPATH/res"
ARSCPATH="$PKGPATH/bin/resources.arsc"
MANPATH="$PKGPATH/bin/AndroidManifest.xml"
TXTMANPATH="$PKGPATH/AndroidManifest.xml"
APTPATH="$PKGPATH/apt"
UNSIGNED_APK="$PKGPATH/unsigned.apk"
SIGNED_APK="$PKGPATH/onscripter-ru.apk"

KEYSTORE="$PKGPATH/Test.keystore"
CERTPATH="$PKGPATH/cert.pem"
KEYPATH="$PKGPATH/key.pem"

echo "Working in $WORK"

rm -rf "$WORK/Droid-package" 
cp -r "$SCRIPTS/../Resources/Droid" "$WORK/Droid-package" || exit 1

if [ -f "$WORK/onscripter-ru" ]; then 
  echo "Proceeding with single arch mode..."
  ARCH="$(basename "$WORK")"
  if [ "$ARCH" == "Droid-arm" ]; then
    echo "Found armeabi-v7a engine, copying..."
    mkdir -p "$PKGPATH/lib/armeabi-v7a" || exit 1
    cp "$WORK/onscripter-ru" "$PKGPATH/lib/armeabi-v7a/libmain.so" || exit 1
  elif [ "$ARCH" == "Droid-aarch64" ]; then
    echo "Found arm64-v8a engine, copying..."
    mkdir -p "$PKGPATH/lib/arm64-v8a" || exit 1
    cp "$WORK/onscripter-ru" "$PKGPATH/lib/arm64-v8a/libmain.so" || exit 1
  elif [ "$ARCH" == "Droid-i386" ]; then
    echo "Found x86 engine, copying..."
    mkdir -p "$PKGPATH/lib/x86" || exit 1
    cp "$WORK/onscripter-ru" "$PKGPATH/lib/x86/libmain.so" || exit 1
  else
    echo "Unknown architecture: $ARCH, check your $WORK folder!"
    exit 1
  fi
else
  echo "Proceeding with multi arch mode..."
  COPIED=false
  if [ -f "$WORK/Droid-arm/onscripter-ru" ]; then
    echo "Found armeabi-v7a engine, copying..."
    mkdir -p "$PKGPATH/lib/armeabi-v7a" || exit 1
    cp "$WORK/Droid-arm/onscripter-ru" "$PKGPATH/lib/armeabi-v7a/libmain.so" || exit 1
    COPIED=true
  fi
  if [ -f "$WORK/Droid-aarch64/onscripter-ru" ]; then
    echo "Found arm64-v8a engine, copying..."
    mkdir -p "$PKGPATH/lib/arm64-v8a" || exit 1
    cp "$WORK/Droid-aarch64/onscripter-ru" "$PKGPATH/lib/arm64-v8a/libmain.so" || exit 1
    COPIED=true
  fi
  if [ -f "$WORK/Droid-i386/onscripter-ru" ]; then
    echo "Found x86 engine, copying..."
    mkdir -p "$PKGPATH/lib/x86" || exit 1
    cp "$WORK/Droid-i386/onscripter-ru" "$PKGPATH/lib/x86/libmain.so" || exit 1
    COPIED=true
  fi
  if ! $COPIED ; then
    echo "Failed to find any engine, aborting!"
    exit 1
  fi
fi

# Further code requires at least one of:
# $PKGPATH/lib/arm64-v8a/libmain.so
# $PKGPATH/lib/armeabi-v7a/libmain.so
# $PKGPATH/lib/x86/libmain.so

compile_sources() {
  echo "Compiling sources..."
  
  OBJPATH="$PKGPATH/obj"
  rm -rf "$OBJPATH"
  
  mkdir -p "$OBJPATH"
  "$JAVAC" -source 1.7 -target 1.7 -classpath "$CLASSPATH" -Xlint:deprecation -d "$OBJPATH" "$SDLACTPATH" "$SDLJPATH" "$SDLAUDIOPATH" "$SDLCTRLPATH" "$ONSACTPATH" "$HIDDEVPATH" "$HIDBLEPATH" "$HIDMGRPATH" "$HIDUSBPATH"

  if (( $? )); then
    exit 1
  fi

  rm -rf "$BINPATH"
  mkdir -p "$BINPATH"
  "$DX" --dex --output="$BINPATH/classes.dex" "$OBJPATH"
  if (( $? )); then
    exit 1
  fi
  
  rm -rf "$OBJPATH"
  echo "Compiling sources successful!"
}

create_apk() {
  echo "Creating APK..."
  rm -f "$UNSIGNED_APK"
  rm -f "$SIGNED_APK"
  
  if [ "$RECOMPILE" != "true" ]; then
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    cp "$MANPATH" "$APTPATH/"
    cp "$BINPATH/classes.dex" "$APTPATH/"
    cp "$ARSCPATH" "$APTPATH/"
    cp -a "$LIBPATH" "$APTPATH/"
    cp -a "$RESPATH" "$APTPATH/"
    
    pushd "$APTPATH" &>/dev/null
    find . -type f -name ".*" | xargs rm -f
    find . -type f -name "Thumbs.db" | xargs rm -f
    
    zip -qry "$UNSIGNED_APK" *
    popd &>/dev/null
        
    rm -rf "$APTPATH"
  else
    "$AAPT" package -f -M "$TXTMANPATH" -S "$RESPATH" -I "$CLASSPATH" -F "$UNSIGNED_APK" "$BINPATH"
    if (( $? )); then
      exit 1
    fi

    pushd "$PKGPATH" &>/dev/null
    if [ -f "lib/arm64-v8a/libmain.so" ]; then
      "$AAPT" add "$UNSIGNED_APK" "lib/arm64-v8a/libmain.so"
    fi
    if [ -f "lib/armeabi-v7a/libmain.so" ]; then
      "$AAPT" add "$UNSIGNED_APK" "lib/armeabi-v7a/libmain.so"
    fi
    if [ -f "lib/x86/libmain.so" ]; then
      "$AAPT" add "$UNSIGNED_APK" "lib/x86/libmain.so"
    fi
    popd &>/dev/null
    
    # Now update binary resources and manifest
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    pushd "$APTPATH" &>/dev/null
    unzip -q "$UNSIGNED_APK"
    
    rm -f "$MANPATH"
    rm -f "$ARSCPATH"
    
    mv "AndroidManifest.xml" "$MANPATH"
    mv "resources.arsc" "$ARSCPATH"
    
    rm -rf "$APTPATH"
  fi
  
  echo "APK creation successful!"
}

sign_apk() {
  echo "Signing APK..."
  if [ "$JAVASIGN" != "true" ]; then
    rm -rf "$APTPATH"
    
    mkdir -p "$APTPATH"
    pushd "$APTPATH" &>/dev/null
    unzip -q "$UNSIGNED_APK"
    
    files=( $(find * -type f) )
    
    mkdir META-INF
    
    # MANIFEST.MF
    printf "Manifest-Version: 1.0\r\n" > META-INF/MANIFEST.MF
    printf "Created-By: 9.6.96 (Xtova Corporation)\r\n\r\n" >> META-INF/MANIFEST.MF
    
    digests=()
    
    for f in "${files[@]}"; do
      hash=$(openssl sha1 -binary "$f" | openssl base64)
      hash256=$(openssl sha256 -binary "$f" | openssl base64)
      digest="Name: $f\r\nSHA-256-Digest: $hash256\r\nSHA1-Digest: $hash\r\n\r\n"
      digests+=("$digest")
      printf "$digest" >> META-INF/MANIFEST.MF
    done
    
    # SELFSIGN.SF
    printf "Signature-Version: 1.0\r\n" > META-INF/SELFSIGN.SF
    hash=$(openssl sha1 -binary "META-INF/MANIFEST.MF" | openssl base64)
    hash256=$(openssl sha256 -binary "META-INF/MANIFEST.MF" | openssl base64)
    printf "SHA-256-Digest-Manifest: $hash256\r\n" >> META-INF/SELFSIGN.SF
    printf "SHA1-Digest-Manifest: $hash\r\n" >> META-INF/SELFSIGN.SF
    printf "Created-By: 9.6.96 (Xtova Corporation)\r\n\r\n" >> META-INF/SELFSIGN.SF
    
    dignum="${#digests[@]}"
    
    for ((i=0; $i<$dignum; i++)); do
      digest="${digests[$i]}"
      file="${files[$i]}"
      hash=$(printf "$digest" | openssl sha1 -binary | openssl base64)
      hash256=$(printf "$digest" | openssl sha256 -binary | openssl base64)
      printf "Name: $file\r\nSHA-256-Digest: $hash256\r\nSHA1-Digest: $hash\r\n\r\n" >> META-INF/SELFSIGN.SF
    done
    
    # SELFSIGN.RSA
    rm -f "$KEYPATH"
    rm -f "$CERTPATH"
    case $(uname) in
      MINGW32*)
        openssl req -x509 -newkey rsa:2048 -keyout "$KEYPATH" -out "$CERTPATH" -days 3650 -nodes -subj "//C=GB\ST=Unknown\L=Unknown\O=Xtova Corporation\OU=Selfsign\CN=Unknown"
        ;;
      *)
        openssl req -x509 -newkey rsa:2048 -keyout "$KEYPATH" -out "$CERTPATH" -days 3650 -nodes -subj "/C=GB/ST=Unknown/L=Unknown/O=Xtova Corporation/OU=Selfsign/CN=Unknown"
        ;;
    esac
    openssl smime -sign -noattr -in META-INF/SELFSIGN.SF -outform der -out META-INF/SELFSIGN.RSA -inkey "$KEYPATH" -signer "$CERTPATH" -md sha1
    if (( $? )); then
      exit 1
    fi
    
    rm -f "$KEYPATH"
    rm -f "$CERTPATH"
    
    zip -qry "$SIGNED_APK" *
    popd &>/dev/null

    rm -rf "$APTPATH"
  else
    rm -f "$KEYSTORE"
    "$JAVAKEY" -genkeypair -validity 10000 \
      -dname "CN=GB, OU=Selfsign, O=Xtova Corporation, L=Unknown, S=Unknown, C=Unknown" \
      -keystore "$KEYSTORE" -storepass password -keypass password -alias Test -keyalg RSA -v
    if (( $? )); then
      exit 1
    fi

    "$JAVASIGNER" -digestalg SHA1 -sigalg SHA1withRSA -keystore "$KEYSTORE" -storepass password -keypass password -signedjar "$SIGNED_APK" "$UNSIGNED_APK" Test
    if (( $? )); then
      exit 1
    fi
    
    rm -f "$KEYSTORE"
  fi
  
  rm -f "$UNSIGNED_APK"
  echo "APK signing successful!"
}

if [ "$RECOMPILE" != "true" ]; then
  command -v zip >/dev/null 2>&1 
  if (( $? )); then
    echo "Unable to find zip, which is required to create apk!"
    exit 1
  fi
fi

if [ "$JAVASIGN" != "true" ]; then
  command -v openssl >/dev/null 2>&1 
  if (( $? )); then
    echo "Unable to find openssl, which is required to sign apk!"
    exit 1
  fi
fi

if [ "$JAVASIGN" == "true" ] || [ "$RECOMPILE" == "true" ]; then
  if [ -z "$JAVA_PATH" ]; then
    JAVAC="$(which javac)"
    if [ "$JAVAC" != "" ]; then
      JAVA_PATH="$(dirname "$JAVAC")"
    fi
    if [ "$JAVA_PATH" == "" ]; then
      pushd "/C/Program Files (x86)/Java/"jdk* &>/dev/null
      if (( $? )); then
        echo "Unable to find JDK, please provide JAVA_PATH variable!"
        popd &>/dev/null
        exit 1
      else
        JAVA_PATH="$(pwd)/bin"
        popd &>/dev/null
      fi
    fi
  fi
  
  echo "Using jdk from $JAVA_PATH"
  
  JAVAC="$JAVA_PATH/javac"
  JAVAKEY="$JAVA_PATH/keytool"
  JAVASIGNER="$JAVA_PATH/jarsigner"

  if [ "$RECOMPILE" == "true" ]; then

    if [ -z "$DROID_TOOLS" ]; then
      pushd "/C/droid/build-tools/"*/ &>/dev/null
      if (( $? )); then
        echo "Unable to find droid build tools, please provide DROID_TOOLS variable!"
        popd &>/dev/null
        exit 1
      else
        DROID_TOOLS="$(pwd)"
        popd &>/dev/null
      fi
    fi

    echo "Using droid build-tools from $DROID_TOOLS"
    
    DX="$DROID_TOOLS/dx"
    AAPT="$DROID_TOOLS/aapt"

    if [ -z "$DROID_PLATFORM" ]; then
      pushd "/C/droid/platforms/android-"*/ &>/dev/null
      if (( $? )); then
        echo "Unable to find droid platform, please provide DROID_PLATFORM variable!"
        popd &>/dev/null
        exit 1
      else
        DROID_PLATFORM="$(pwd)"
        popd &>/dev/null
      fi
    fi

    echo "Using droid droid platform from $DROID_PLATFORM"
    
    if [ ! -f "$DX" ]; then
      if [ -f "$DX.bat" ]; then
        DX="$DX.bat"
      elif [ -f "$DX.exe" ]; then
        DX="$DX.exe"
      elif [ -f "$DX.sh" ]; then
        DX="$DX.sh"
      else
        echo "Unable to find dx tool, please provide valid DROID_PLATFORM variable!"
        exit 1
      fi
    fi

    CLASSPATH="$DROID_PLATFORM/android.jar"

    compile_sources
  fi
fi

create_apk
sign_apk

echo "Please grab your apk at $SIGNED_APK"

exit 0
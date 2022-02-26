#!/bin/bash

#
# ndktoolchain.sh
# ONScripter-RU
#
# Droid NDK toolchain downloader script.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

srcdir=$(dirname "$0")
source "$srcdir/../Dependencies/common.sh"

if [ "$#" != "1" ]; then
    error "usage: ./ndktoolchain.sh <path>"
    exit 1
fi

dstdir="$1"
ndkpath="$dstdir/ndk"

if [ "$PYTHON" == "" ]; then
    PYTHON="python"
fi

ndkver="r20"
ndkrel="1"
ndkstl="libc++"
ndkhash="invalid"
ndkgood=true

# Keep these in sync
ndkarch=(
    "arm"
    "arm64"
    "x86"
)
ndkabi=(
    "arm-linux-androideabi"
    "aarch64-linux-android"
    "i686-linux-android"
)
# 16 is the minimal requirement for posix_memalign (4.1)
# 18 is the minimal requirement for GLES 3.0 (4.3)
# 21 is the minimal requirement for aarch64 (5.1)
ndkapi=(
    "16"
    "21"
    "16"
)
archnum="${#ndkarch[@]}"

for ((i=0; $i<$archnum; i++)); do 
    arch="${ndkarch[$i]}"
    api="${ndkapi[$i]}"
    abi="${ndkabi[$i]}"

    if [ ! -f "$dstdir/ndk/toolchain-$arch/triple" ] || [ "$(cat $dstdir/ndk/toolchain-$arch/triple)" != "$abi" ]; then
        # warn "Found non-compliant ndk triple for $abi, rebuilding!"
        ndkgood=false
        break
    fi

    if [ ! -f "$dstdir/ndk/toolchain-$arch/version" ] || [ "$(cat $dstdir/ndk/toolchain-$arch/version)" != "${ndkver}-${ndkrel}" ]; then
        # warn "Found non-compliant ndk version for ${ndkver}-${ndkrel}, rebuilding!"
        ndkgood=false
        break
    fi
done

if $ndkgood; then
    # This ndk and its toolchains are already recent.
    exit 0
fi

msg "Starting to setup ndk in $ndkpath..."

case $(uname) in
    Darwin*)
        platform="darwin"
        platformcpu="x86_64"
        ndkhash="2ec06c4576c6ad50a727f0a5fbd0f67563aa6e8b348cff904b029622a470f2f1"
        ;;
    MINGW*)
        platform="windows"
        platformcpu="x86"
        ndkhash="7541bacd22f5757b9947314ee71111e18fc7db852ac67b23b7dbace229b941cf"
        ;;
    Linux*)
        platform="linux"
        platformcpu="x86_64"
        ndkhash="57435158f109162f41f2f43d5563d2164e4d5d0364783a9a6fab3ef12cb06ce0"
        ;;
esac

ndkdir="android-ndk-${ndkver}"
ndkpackage="${ndkdir}-${platform}-${platformcpu}.zip"
ndkurl="https://dl.google.com/android/repository/${ndkpackage}"

mkdir -p "$ndkpath"
if [ ! -f "$ndkpath/$ndkpackage" ]; then
    msg "Downloading ndk..."
    if [ "$(which wget)" != "" ]; then
        wget -O "$ndkpath/$ndkpackage" "$ndkurl"
    else
        curl -o "$ndkpath/$ndkpackage" "$ndkurl"
    fi
    if [ ! -f "$ndkpath/$ndkpackage" ]; then
        error "Unable to download ndk!"
        exit 1
    fi
fi


if [ "$(which shasum)" != "" ]; then
    nhash=$(shasum -a 256 "$ndkpath/$ndkpackage" | cut -f1 -d' ')
else
    nhash=$(openssl sha256 "$ndkpath/$ndkpackage" | cut -f2 -d' ')
fi

if [ "$nhash" != "$ndkhash" ]; then
    error "Invalid ndk hash ${nhash}, expected ${ndkhash}"
    rm -f "$ndkpath/$ndkpackage"
    exit 1
fi

msg "Extracting ndk..."
rm -rf "$ndkpath/$ndkdir"
ret=0
unzip -q "$ndkpath/$ndkpackage" -d "$ndkpath" || ret=1

if [ "$ret" == "1" ] || [ ! -d "$ndkpath/$ndkdir" ]; then
    error "Unable to extract ndk!"
    exit 1
fi

for ((i=0; $i<$archnum; i++)); do 
    arch="${ndkarch[$i]}"
    api="${ndkapi[$i]}"
    abi="${ndkabi[$i]}"

    msg "Making $arch ($api/$abi) toolchain in $dstdir/ndk/toolchain-$arch"
    rm -rf "$dstdir/ndk/toolchain-$arch"
    ret=0
    $PYTHON "$ndkpath/$ndkdir/build/tools/make_standalone_toolchain.py" --arch "$arch" --install-dir="$dstdir/ndk/toolchain-$arch" --stl "$ndkstl" --api "$api" || ret=1
    if [ "$ret" == "1" ] || [ ! -d "$dstdir/ndk/toolchain-$arch" ]; then
        error "Unable to install ndk!"
        exit 1
    fi
    echo "$abi" > "$dstdir/ndk/toolchain-$arch/triple"
    echo "${ndkver}-${ndkrel}" > "$dstdir/ndk/toolchain-$arch/version"
done

rm -rf "$ndkpath/$ndkdir"
rm -f "$ndkpath/${ndkpackage}"
# cp -a "$srcdir/../droid/package" "$dstdir/"

exit 0

#!/bin/bash

# This program builds software by following instructions in .pkgbuild files.
# It borrows heavy inspiration from Arch Linux's makepkg script.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# These variables define key paths used by the script.

startdir="$PWD"
srcdir="$startdir/src"
outdir="$startdir/onscrlib"
logdir="$startdir/log"
buildpkg="$startdir/build.sh"
dldir="$startdir/download"
patchdir="$startdir/patches"

source common.sh

# Documentation.

show_help() {
cat <<EndOfHelp
usage: ./build.sh [options] package

    -O <str>    sets named option <str>.  May be specified multiple times for
                multiple options.

    -d          enables debug mode; displays additional output about what the
                build script is doing

    -e          skips the extraction step and expects an extracted copy of the
                tarballs to already exist in the src directory

    -p          skips the patching step

    -f          forces rebuilding this package if a compiled version already
                exists in the output directory

    -r <num>    forces rebuilding this package and any dependencies
                up to <num> levels deep if compiled versions already exist
                in the output directory.  (Note: -r 0 is equivalent to -f)

    -o <dir>    sets the output directory (the place where packages will be
                installed) to <dir>

    -l          displays all the packeges with their version numbers that are
                currently installed to onscrlib folder

    -b <pkg>    checks if a package depends on a specified package
                system use only

    -c <triple> sets cross compilation triple to the specified value, you are
                also responsible to provide valid toolchain environment vars
                (CC, CXX, LD, AR, LIBTOOL, etc.)

    -g <path>   builds for the beloved Android with the toolchain from path

    -h          displays this help message
EndOfHelp
case $(uname) in
    Darwin*)
cat <<EndOfHelp

  MAC-SPECIFIC OPTIONS:

    -i          build for iOS. You are inspired to specify a correct architecture.
                System default compilers will be used.

    -m  <ver>   sets the version of iOS or macOS to build for.  The appropriate
                SDKs and compilers must be installed.  Defaults to 10.6.

    -a  <arch>  sets the iOS or macOS architecture to build for.  May be specified
                multiple times to pass multiple -arch flags to the compiler.
                Defaults to i386.
EndOfHelp
    ;;
    esac
}

list_packages() {
    msg_start "Listing all installed packages"
    if [ -d "$outdir/.pkgs" ]; then
        for i in $outdir/.pkgs/* ; do
            if [ "$(basename $i)" != "onscrlib" ]; then
                ver=$(trim $(<"$i"))
                msg "$(basename $i) $ver"
            fi
        done
    else
        warn "No installed packages found"
    fi
    msg_stop "Done"
}

# Handle all of the options passed to the script.

DEBUG=false
MMAC_VER_MIN=10.6
MAC_SDK=
MAC_MIN_VER=1060
APPLE_ARCH=()
MIOS_VER_MIN=8.0
IOS_MIN_VER=800
IOS_SDK=8.4
DROID_DEFAULT_TRIPLE="arm-linux-androideabi"
CROSS_BUILD=false
CROSS_SHIM=true
CROSS_TRIPLE=""
CROSS_TARGET=""
CROSS_SYS_PREFIX=""
ORIG_ARGS=(${@:1:$(($# - 1))})
SKIP_EXTRACT=false
SKIP_PATCH=false
FORCE_BUILD=false
FORCE_RECURSION_LEVEL=0
OPTIONS_EXTRA=()
OPTIONS_REMOVE=()
DEPEND_CHECK=""
SLIM_BUILD=true
USE_CURL=false
USE_OPENSSL=false

while getopts O:defphilm:a:o:r:b:c:g: o; do
    case "$o" in
        O)
            if [ ${OPTARG:0:1} = '-' ]; then
                OPTIONS_REMOVE=(${OPTIONS_REMOVE[@]} ${OPTARG:1})
            elif [ ${OPTARG:0:1} = '+' ]; then
                OPTIONS_EXTRA=(${OPTIONS_EXTRA[@]} ${OPTARG:1})
            else
                OPTIONS_EXTRA=(${OPTIONS_EXTRA[@]} $OPTARG)
            fi;;
        d)
            DEBUG=true;;
        e)
            SKIP_EXTRACT=true;;
        f)
            FORCE_BUILD=true;;
        p)
            SKIP_PATCH=true;;
        l)
            list_packages
            exit 0;;
        h)
            show_help
            exit 0;;
        r)
            FORCE_BUILD=true
            FORCE_RECURSION_LEVEL=$(( $OPTARG + 0 ))
            if [ $(( $FORCE_RECURSION_LEVEL )) -lt 0 ]; then
                FORCE_RECURSION_LEVEL=0
            fi;;
        m)
            MMAC_VER_MIN="$OPTARG"
            MAC_MIN_VER="${MMAC_VER_MIN//./}0"
            MIOS_VER_MIN="$OPTARG"
            IOS_MIN_VER="${MIOS_VER_MIN//./}0" ;;
        a)
            APPLE_ARCH=( ${APPLE_ARCH[@]} "-arch $OPTARG" );;
        b)
            DEPEND_CHECK="$OPTARG";;
        c)
            CROSS_BUILD=true
            CROSS_TRIPLE="$OPTARG";;
        i)
            CROSS_BUILD=true
            CROSS_SHIM=false
            CROSS_TRIPLE="arm-apple-darwin"
            CROSS_TARGET="darwin-iOS"
            PATH="$(pwd)/path:$PATH"
            CC=clang
            CXX=clang++
            ;;
        g)
            CROSS_BUILD=true
            CROSS_TARGET="droid"
            if [ "$CROSS_TRIPLE" == "" ]; then
                CROSS_TRIPLE="$(cat $OPTARG/triple)"
                if [ "$CROSS_TRIPLE" == "" ]; then
                    CROSS_TRIPLE=$DROID_DEFAULT_TRIPLE
                    warn "No triple file found in the provided ndk, falling back to $CROSS_TRIPLE"
                fi
            fi
            PATH="$OPTARG/bin:$PATH"
            # Full path is necessary on Windows due to $PATH not being synced with %PATH%
            if [[ $(uname) == MINGW* ]]; then
                CROSS_SYS_PREFIX="$OPTARG/bin/"
            fi
            CC="${CROSS_SYS_PREFIX}clang"
            CXX="${CROSS_SYS_PREFIX}clang++"
            ;;
        o)
            ret=0
            mkdir -p "$OPTARG" &&
            pushd "$OPTARG" &> /dev/null || ret=1
            if (( $ret )); then
                error "Failed to set output directory."
                exit 1
            fi
            outdir=`pwd`
            popd &>/dev/null
            unset ret;;
        \?)
            error "Unknown option -%s" "$OPTARG"
            exit 1;;
        :)
            error "Option -%s requires an argument." "$OPTARG"
            exit 1;;
    esac
done

if [ ${#APPLE_ARCH[@]} -eq 0 ]; then
    if $CROSS_BUILD; then
        APPLE_ARCH=("-arch armv7s")
    else
        APPLE_ARCH=("-arch x86_64")
    fi
fi

if [ ! ${#APPLE_ARCH[@]} -eq 1 ]; then
    warn "Producing a fat binary may not be possible for some packages"
    SLIM_BUILD=false
fi

shift $(( $OPTIND - 1 ))

# Determine what we should use for file downloading.

if [ -z `which wget 2>/dev/null` ]; then
    USE_CURL=true
    # We could test if we actually have curl, but instead we'll just fail
    # later on if the script tries to download something (this allows us
    # to continue without either tool if the package doesn't require
    # downloading)
fi

if [ -z `which shasum 2>/dev/null` ]; then
    USE_OPENSSL=true
fi

# Verify that we've been given the name of a package.

if [ $# -ne 1 ]; then
    error "Either the name of a package or the path to a pkgbuild file must"
    error "be specified on the command line."
    exit 1
fi

# Functions for manipulating file names, paths, and URLs.

get_filepath() {
    local file="$(get_filename "$1")"

    if [[ -f "$startdir/$file" ]]; then
        file="$startdir/$file"
    elif [[ -f "$patchdir/$file" ]]; then
        file="$patchdir/$file"
    elif [[ -f "$dldir/$file" ]]; then
        file="$dldir/$file"
    else
        return 1
    fi

    echo "$file"
}

get_filename() {
    local filename="${1%%::*}"
    echo "${filename##*/}"
}

get_url() {
    echo "${1#*::}"
}

# Utility to check and see if something exists in the given array.

in_array() {
    local needle=$1; shift
    [[ -z $1 ]] && return 1 # Not Found
    local item
    for item in "$@"; do
        [[ $item = $needle ]] && return 0 # Found
    done
    return 1 # Not Found
}

# Check if the given option is set

option_set() {
    in_array "$1" ${OPTIONS_EXTRA[@]} ${options[@]}
    a=$?
    in_array "$1" ${OPTIONS_REMOVE[@]}
    b=$?
    test $a = 0 -a ! $b = 0
    return $?
}

# Function to verify file hash.

verify_file() {
    local file=$1
    local hash=$2

    if $USE_OPENSSL; then
        local nhash=$(openssl sha256 "$file" | cut -f2 -d' ')
    else
        local nhash=$(shasum -a 256 "$file" | cut -f1 -d' ')
    fi

    if [ "$nhash" != "$hash" ]; then
        error_out "%s has wrong hash %s, expected %s." "$file" "$nhash" "$hash"
    fi
}

# Function to download a file.

download_file() {
    local url=$1
    local file=$2

    # Somtimes Travis CI networking starts to fail to resolve domains
    # out of a sudden. Retry a few more times.
    for n in {1..5}; do
        local ret=0
        if [ $USE_CURL ]; then
            curl -L "$url" -o "$dldir/$file" || ret=$?
        else
            wget "$url" -O "$dldir/$file" || ret=$?
        fi
        if (( $ret )); then
            rm -f "$dldir/$file"
            warn "Failed to download %s, retrying" "$file"
            sleep 1
        else
            return
        fi
    done
    error_out "Failed to download %s" "$file"
}

# Fetch all of the things in the $sources[@] array.  If they already exist
# locally, the local copies will be used instead.

download_sources() {
    msg "Retrieving sources"

    for ((i=0; $i < ${#sources[@]}; i++)); do
        local netfile="${sources[$i]}"
        local file=$(get_filepath "$netfile" || true)
        if [[ -n "$file" ]]; then
            msg2 "Found %s" "${file##*/}"
            verify_file "$file" "${hashes[$i]}"
            rm -f "$srcdir/${file##*/}"
            ln -s "$file" "$srcdir/"
            continue
        fi

        file=$(get_filename "$netfile")
        local url=$(get_url "$netfile")

        if [[ $file = $url ]]; then
            error_out "%s was not found in the build directory and is not a URL." "$file"
        fi

        local ret=0
        msg2 "Downloading %s" "$file"
        download_file "$url" "$file"
        verify_file "$dldir/$file" "${hashes[$i]}"
        rm -f "$srcdir/${file##*/}"
        ln -s "$dldir/$file" "$srcdir/$file"
    done
}

# Extract all of the sources ending in the appropriate extensions, provided
# that they are not in the $noextract[@] array.

extract_sources() {
    msg "Extracting sources"
    pushd $srcdir &>/dev/null

    local netfile
    for netfile in "${sources[@]}"; do
        local file=$(get_filename "$netfile")
        if in_array "$file" "${noextract[@]}"; then
            continue
        fi

        local filetype=$(file -bzL --mime "$file")
        local ext=${file##*.}
        local cmd=''
        local cmd_flags=''

        case "$filetype" in
            *application/x-tar*)
                cmd='tar -xf';;
            *application/x-zip*|*application/zip*)
                cmd='unzip -q';;
            *)
                # MinGW32 has broken mime types in 'file' command, fall back on the
                # extension to work around
                case "$ext" in
                    bz2|gz|tar|xz)
                        # should look one more level in, to see if tar is there...
                        cmd='tar -xf';;
                    zip)
                        # should look one more level in, to see if tar is there...
                        cmd='unzip -q';;
                    *)
                        continue;;
                esac;;
        esac

        local ret=0
        msg2 "Extracting %s with %s" "$file" "$cmd"
        $cmd "$file" || ret=$?

        if (( ret )); then
            error_out "Failed to extract %s" "$file"
        fi
    done

    popd &>/dev/null
}

# Removes outdated source files
cleanup_sources() {
    for i in $( find "$srcdir" -name "${pkgname}-*" ); do
        rm -rf $i
    done
}


# Applies a patch (assume -p1) and logs the output.

apply_patch() {
    local patchfile="$1"
    if $SKIP_PATCH; then
        msg2 "Skipping %s" "$patchfile"
    else
        local patchpath="$(get_filepath "$patchfile")"
        local logfile="$logdir/$pkgname.$(get_filename "$patchfile").log"
        msg2 "Applying %s" "$patchfile"
        local ret=0
        patch -p1 < "$patchpath" >$logfile || ret=$?
        if (( $ret )); then
            error "Failed to apply %s" "$patchfile"
            exit 1
        fi
    fi
}

# Executes prior to configuring.

prebuild() {
    pushd "$pkgname-$pkgver" &>/dev/null
}

# Run a script called "autogen.sh" in the source directory, logging the
# output and trapping failure.

autogen() {
    msg2 "Generating configure script"
    local logfile="$logdir/$pkgname.autogen.log"
    local ret=0
    ./autogen.sh &>"$logfile" || ret=$?
    if (( $ret )); then
        tail -n 20 "$logfile"
        error "Generating configure script failed"
        exit 1
    fi
}

# Runs the configure script, with the appropriate arguments.

configure() {
    local logfile="$logdir/$pkgname.configure.log"
    export CFLAGS="$CFLAGS ${cflags[@]} $CFLAGS_EXTRA"
    export CPPFLAGS="$CPPFLAGS ${cppflags[@]} $CPPFLAGS_EXTRA"
    export LDFLAGS="$LDFLAGS ${ldflags[@]} $LDFLAGS_EXTRA"
    if $DEBUG; then
        msg2 "CFLAGS:  %s" "${CFLAGS[@]}"
        msg2 "CPPFLAGS:  %s" "${CPPFLAGS[@]}"
        msg2 "LDFLAGS: %s" "${LDFLAGS[@]}"
        msg2 "option:   %s" "${configopts[@]}"
    fi
    local ret=0
    ./configure --prefix="$outdir" ${configopts[@]} &>"$logfile" || ret=$?
    if (( $ret )); then
        tail -n 20 "$logfile"
        error "Configuring %s failed" "$pkgname"
        error "The last 20 lines of the log are shown above."
        error "The full log is: %s" "$logfile"
        exit 1
    fi
    unset CFLAGS
    unset CPPFLAGS
    unset LDFLAGS
}

compile() {
    local logfile="$logdir/$pkgname.compile.log"
    local ret=0
    make $MAKEOPTS &>"$logfile" || ret=$?
    if (( $ret )); then
        tail -n 20 "$logfile"
        error "Compiling %s failed" "$pkgname"
        error "The last 20 lines of the log are shown above."
        error "The full log is: %s" "$logfile"
        exit 1
    fi
}

copy() {
    local logfile="$logdir/$pkgname.install.log"
    local ret=0
    make install &>"$logfile" || ret=$?
    if (( $ret )); then
        tail -n 20 "$logfile"
        error "Installing %s failed" "$pkgname"
        error "The last 20 lines of the log are shown above."
        error "The full log is: %s" "$logfile"
        exit 1
    fi
}

postbuild() {
    popd &>/dev/null
}

checkdepends() {
    local ret=0
    if [ -d "$outdir/.pkgs" ]; then
        for i in $outdir/.pkgs/* ; do
            # A hack for freetype
            if [[ "$(basename $i)" != "harfbuzz" || "$pkgname" != "freetype" ]]; then
                if [ "$(basename $i)" != "onscrlib" ]; then
                    ret=0
                    $buildpkg ${ORIG_ARGS[@]} -b $pkgname $(basename $i) || ret=$?
                    if (( $ret )); then
                        if $DEBUG; then
                            msg "Invalidating"
                        fi
                        echo -n "invalid" >"$outdir/.pkgs/$(basename $i)"
                        export ONSCRLIB_INVALID=1
                    fi
                fi
            fi
        done
    fi
}

finalise() {
    return
}

build() {
    msg "Performing pre-build actions"
    prebuild

    msg "Configuring %s" "$pkgname"
    configure

    msg "Compiling %s" "$pkgname"
    compile

    msg "Installing %s" "$pkgname"
    copy

    msg "Finishing %s" "$pkgname"
    postbuild
}

pkgbuild=$1

if [ ! -f "$pkgbuild" ]; then
    pkgbuild="$startdir/pkgs/$pkgbuild.pkgbuild"
fi

if [ ! -f "$pkgbuild" ]; then
    error "Unknown package '%s'" "$1"
    exit 1
fi

getHost() {
    case $(uname) in
        Darwin*) echo "darwin-macOS" ;;
        MINGW32*) echo "win32" ;;
        *) echo "linux-like" ;; # For bsd compat
    esac
}

getHostPrefix() {
    if $CROSS_BUILD; then
        echo $CROSS_TRIPLE
    else
        echo ""
    fi
}

getTarget() {
    if $CROSS_BUILD; then
        if [ "$CROSS_TARGET" != "" ]; then
            echo $CROSS_TARGET
        else
            case $CROSS_TRIPLE in
                *darwin*) echo "darwin-macOS" ;;
                *mingw32*) echo "win32" ;;
                *) echo "linux-like" ;; # For bsd compat
            esac
        fi
    else
        getHost
    fi
}

getTargetCPU() {
    case $(getTarget) in
        darwin-macOS)
            if [ "$APPLE_CPU_FLAG" == "-m32" ]; then
                echo "i686"
            else
                echo "x86_64"
            fi
        ;;
        darwin-iOS)
            if [ "$APPLE_CPU_FLAG" == "-m32" ]; then
                if in_array "$APPLE_ARCH" "-arch armv7s"; then
                    echo "armv7s"
                else
                    echo "armv7"
                fi
            else
                echo "arm64"
            fi
        ;;
        win32)
            echo "i686"
        ;;
        linux-like)
            if [ "$(uname -m)" == "x86_64" ]; then
                echo "x86_64"
            elif [ "$(uname -m)" == "aarch64" ]; then
                echo "aarch64"
            else
                echo "i686"
            fi
        ;;
        droid)
            if [[ $CROSS_TRIPLE == *86* ]]; then 
                echo "x86_32"
            elif [[ $CROSS_TRIPLE == *aarch64* ]]; then
                echo "aarch64"
            else
                echo "arm"
            fi
        ;;
        *)
            echo "unknown"
        ;;
    esac
}

getCC() {
    if [ "$CC" != "" ]; then
        echo "$CC"
        return
    fi

    case $(getTarget) in
        darwin*|droid) echo "clang" ;;
        *) echo "gcc" ;;
    esac
}

getCXX() {
    if [ "$CXX" != "" ]; then
        echo "$CXX"
        return
    fi

    case $(getTarget) in
        darwin*|droid) echo "clang++" ;;
        *) echo "g++" ;;
    esac
}

getArch() {
    echo "${APPLE_ARCH[*]}"
}

# Find MacOSX SDK
case $(getHost) in
    darwin-macOS)
        # SDKs are in a stupid new place on Lion
        if [ -d /Developer/SDKs ]; then
            MAC_SDK_PATH=/Developer/SDKs
            IOS_SDK_PATH=/Developer/SDKs
        else
            MAC_SDK_PATH="`xcode-select -print-path`/Platforms/MacOSX.platform/Developer/SDKs"
            IOS_SDK_PATH="`xcode-select -print-path`/Platforms/iPhoneOS.platform/Developer/SDKs"
        fi

        if [ ! -d "${MAC_SDK_PATH}/MacOSX${MAC_SDK}.sdk" ]; then
            #warn "Failed to find a specified MacOSX SDK, performing a search"
            MAC_SDK=""

            for i in {6..15}; do
                if [ -d "${MAC_SDK_PATH}/MacOSX10.${i}.sdk" ]; then
                    MAC_SDK="10.${i}"
                fi
            done

            if [ "${MAC_SDK}" == "" ]; then
                error_out "No installed MacOSX SDK found, cannot continue"
            fi

            if [ "${MAC_SDK}" == "10.6" ]; then
                warn "Using outdated 10.6 SDK, packages like SDL2 will not build"
            fi

            #msg "Search succeeded with ${MAC_SDK} SDK"
        fi

        if [ "${MAC_SDK}" == "10.6" ]; then
            warn "Using outdated 10.6 SDK, packages like SDL2 will not build"
        fi

        if [ ! -d "${IOS_SDK_PATH}/iPhoneOS${IOS_SDK}.sdk" ]; then
            #warn "Failed to find a specified iOS SDK, performing a search"
            IOS_SDK=""

            for i in {8..15}; do
                for j in {0..6}; do
                    if [ -d "${IOS_SDK_PATH}/iPhoneOS${i}.${j}.sdk" ]; then
                        IOS_SDK="${i}.${j}"
                    fi
                done
            done

            if [ "${IOS_SDK}" == "" ]; then
                error_out "No installed iOS SDK found, cannot continue"
            fi

            #msg "Search succeeded with ${MAC_SDK} SDK"
        fi
    ;;
esac

# Default Mac flags, can be overridden
# Better use _extra
configopts_mac=(
    "CC=$(getCC)"
    "CXX=$(getCXX)"
)

cflags_mac=(
    "-O"
    "-isysroot ${MAC_SDK_PATH}/MacOSX${MAC_SDK}.sdk"
    "-F${MAC_SDK_PATH}/MacOSX${MAC_SDK}.sdk/System/Library/Frameworks"
    "${APPLE_ARCH[@]}"
    "-mmacosx-version-min=${MMAC_VER_MIN}"
)

cppflags_mac=(
    "-DMAC_OS_X_VERSION_MIN_REQUIRED=${MAC_MIN_VER}"
)

ldflags_mac=(
    "-Wl,-syslibroot,${MAC_SDK_PATH}/MacOSX${MAC_SDK}.sdk"
    "-F${MAC_SDK_PATH}/MacOSX${MAC_SDK}.sdk/System/Library/Frameworks"
    "${APPLE_ARCH[@]}"
    "-mmacosx-version-min=${MMAC_VER_MIN}"
)

# Default iOS flags, can be overridden
# Better use _extra
configopts_ios=(
    "CC=$(getCC)"
    "CXX=$(getCXX)"
)

cflags_ios=(
    "-O"
    "-isysroot ${IOS_SDK_PATH}/iPhoneOS${IOS_SDK}.sdk"
    "-F${IOS_SDK_PATH}/iPhoneOS${IOS_SDK}.sdk/System/Library/Frameworks"
    "${APPLE_ARCH[@]}"
    "-mios-version-min=${MIOS_VER_MIN}"
)

cppflags_ios=(
    "-DIPHONE_OS_VERSION_MIN_REQUIRED=${IOS_MIN_VER}"
)

ldflags_ios=(
    "-Wl,-syslibroot,${IOS_SDK_PATH}/iPhoneOS${IOS_SDK}.sdk"
    "-F${IOS_SDK_PATH}/iPhoneOS${IOS_SDK}.sdk/System/Library/Frameworks"
    "${APPLE_ARCH[@]}"
    "-mios-version-min=${MIOS_VER_MIN}"
)

if $SLIM_BUILD; then
    if in_array "$APPLE_ARCH" "-arch i386"; then
        export APPLE_CPU_FLAG="-m32"
    elif in_array "$APPLE_ARCH" "-arch x86_64"; then
        export APPLE_CPU_FLAG="-m64"
    elif in_array "$APPLE_ARCH" "-arch armv7"; then
        export APPLE_CPU_FLAG="-m32"
    elif in_array "$APPLE_ARCH" "-arch armv7s"; then
        export APPLE_CPU_FLAG="-m32"
    elif in_array "$APPLE_ARCH" "-arch arm64"; then
        export APPLE_CPU_FLAG="-m64"
    fi

    cflags_mac=( ${cflags_mac[@]} "$APPLE_CPU_FLAG" )
    cppflags_mac=( ${cppflags_mac[@]} "$APPLE_CPU_FLAG" )
    ldflags_mac=( ${ldflags_mac[@]} "$APPLE_CPU_FLAG" )

    cflags_ios=( ${cflags_ios[@]} "$APPLE_CPU_FLAG" )
    cppflags_ios=( ${cppflags_ios[@]} "$APPLE_CPU_FLAG" )
    ldflags_ios=( ${ldflags_ios[@]} "$APPLE_CPU_FLAG" )
fi

addcrossopt=true

source "$pkgbuild"

cflags_mac=( ${cflags_mac[@]} ${cflags_mac_extra[@]} )
cppflags_mac=( ${cppflags_mac[@]} ${cppflags_mac_extra[@]} )
ldflags_mac=( ${ldflags_mac[@]} ${ldflags_mac_extra[@]} )
cflags_ios=( ${cflags_ios[@]} ${cflags_ios_extra[@]} )
cppflags_ios=( ${cppflags_ios[@]} ${cppflags_ios_extra[@]} )
ldflags_ios=( ${ldflags_ios[@]} ${ldflags_ios_extra[@]} )
configopts_mac=( ${configopts_mac[@]} ${configopts_mac_extra[@]} )
configopts_ios=( ${configopts_ios[@]} ${configopts_ios_extra[@]} )

if [ "$DEPEND_CHECK" != "" ]; then
    for dep in "${depends[@]}"; do
        if [ "$dep" == "$DEPEND_CHECK" ]; then
            if $DEBUG; then
                msg "$pkgname does depend on $DEPEND_CHECK"
            fi
            exit 1
        fi
    done
    if $DEBUG; then
        msg "$pkgname does not depend on $DEPEND_CHECK"
    fi
    exit 0
fi

if [ $(( $PKGBUILD_RECURSION_DEPTH )) -gt 10 ]; then
    error "Maximum recursion depth exceeded; there is probably a circular"
    error "dependency."
    exit 1
fi

if [ -f "$outdir/.pkgs/$pkgname" ]; then
    # Compare installed versions as well
    ver=$(trim $(<"$outdir/.pkgs/$pkgname"))
    if [[ "$pkgver-$pkgrel" != "$ver" || "$pkgname" == "onscrlib" ]]; then
        msg "Rebuilding %s" "$pkgname"
    elif $FORCE_BUILD; then
        if [ $(( $PKGBUILD_RECURSION_DEPTH )) -gt $(( $FORCE_RECURSION_LEVEL )) ]; then
            exit 0
        fi
        msg "Rebuilding %s" "$pkgname"
    else
        if [ $(( $PKGBUILD_RECURSION_DEPTH )) -eq 0 ]; then
            msg "Skipping build of %s (already done)" "$pkgname"
        fi
        exit 0
    fi
fi

export PKGBUILD_RECURSION_DEPTH=$(( $PKGBUILD_RECURSION_DEPTH + 1 ))

for dep in "${depends[@]}"; do
    if $DEBUG; then
        msg "%s depends on: %s" "$pkgname" "$dep"
        msg "[%d]Recursive invocation: %s" $PKGBUILD_RECURSION_DEPTH \
                                           "$buildpkg ${ORIG_ARGS[@]} $dep"
    fi
    ret=0
    $buildpkg ${ORIG_ARGS[@]} $dep || ret=$?
    if (( $ret )); then
        error "Failed to build dependency %s" "$dep"
        error "Aborting."
        exit 1
    fi
done

cflags=(${cflags[@]} "-I$outdir/include")
cppflags=(${cppflags[@]} "-I$outdir/include")
ldflags=(${ldflags[@]} "-L$outdir/lib")

case $(getTarget) in
    linux-like)
        configopts=(${configopts_linux[@]} ${configopts[@]})
        cflags=(${cflags_linux[@]} ${cflags[@]})
        cppflags=(${cppflags_linux[@]} ${cppflags})
        ldflags=(${ldflags_linux[@]} ${ldflags[@]})
        ;;
    darwin-iOS)
        configopts=(${configopts_ios[@]} ${configopts[@]})
        cflags=(${cflags_ios[@]} ${cflags[@]})
        cppflags=(${cppflags_ios[@]} ${cppflags})
        ldflags=(${ldflags_ios[@]} ${ldflags[@]})
        ;;
    darwin-macOS)
        configopts=(${configopts_mac[@]} ${configopts[@]})
        cflags=(${cflags_mac[@]} ${cflags[@]})
        cppflags=(${cppflags_mac[@]} ${cppflags})
        ldflags=(${ldflags_mac[@]} ${ldflags[@]})
        ;;
    win32)
        configopts=(${configopts_win32[@]} ${configopts[@]})
        cflags=(${cflags_win32[@]} ${cflags[@]})
        cppflags=(${cppflags_win32[@]} ${cppflags})
        ldflags=(${ldflags_win32[@]} ${ldflags[@]})
        ;;
    droid)
        configopts=(${configopts_droid[@]} ${configopts[@]})
        cflags=(${cflags_droid[@]} ${cflags[@]})
        cppflags=(${cppflags_droid[@]} ${cppflags})
        ldflags=(${ldflags_droid[@]} ${ldflags[@]})
        ;;
    *)
        warn "Building for unknown platform"
        ;;
esac

export PATH="$outdir/bin:$PATH"

# This is mainly necessary for win32 bzip2 and ffmpeg compilation
mkdir -p $outdir/bin
echo "$(getCC) \"\$@\"" > $outdir/bin/ccreal
echo "$(getCXX) \"\$@\"" > $outdir/bin/cxxreal
chmod a+x $outdir/bin/ccreal $outdir/bin/cxxreal

if $CROSS_BUILD; then
    if $CROSS_SHIM; then
        # Makes sure zlib's terrible idea to look for Apple in libtool version fails.
        echo "echo \"This is NOT Apple libtool! Called with:\"" > $outdir/bin/libtool
        echo "${CROSS_SYS_PREFIX}$(getHostPrefix)-ar cru \"\$@\"" >> $outdir/bin/libtool
        chmod a+x $outdir/bin/libtool
        # Makes sure the right ranlib is used
        echo "${CROSS_SYS_PREFIX}$(getHostPrefix)-ranlib \"\$@\"" > $outdir/bin/ranlib
        chmod a+x $outdir/bin/ranlib
        # Makes sure the right ar is used (ffmpeg, SDL2_gpu)
        echo "${CROSS_SYS_PREFIX}$(getHostPrefix)-ar \"\$@\"" > $outdir/bin/ar
        chmod a+x $outdir/bin/ar
        # Makes sure the right strip is used (lua)
        echo "${CROSS_SYS_PREFIX}$(getHostPrefix)-strip \"\$@\"" > $outdir/bin/strip
        chmod a+x $outdir/bin/strip
        # Additinally adds gas-preprocessor (ffmpeg)
        cp -f "$(pwd)/path/gas-preprocessor.pl" $outdir/bin/
        cp -f "$(pwd)/path/clang-as-armv7s" $outdir/bin/
        cp -f "$(pwd)/path/clang-as-armv7" $outdir/bin/
        chmod a+x "$outdir/bin/gas-preprocessor.pl" "$outdir/bin/clang-as-armv7s" "$outdir/bin/clang-as-armv7"
    fi
    # Also sets host for configure options
    if $addcrossopt; then
        configopts+=("--host=$(getHostPrefix)")
    fi
fi

# Enable boosted compilation with clang or anything but MINGW on Windows
# GCC on Windows causes build failures otherwise.
if [ "$(getCC)" != "gcc" ] || [ "$(getHost)" != "win32" ]; then
    MAKEOPTS="$MAKEOPTS -j $(getconf _NPROCESSORS_ONLN)"
fi

if [[ ! "$type" = "meta" ]]; then
    msg_start "Building %s" "$pkgname"
    mkdir -p "$srcdir" "$outdir" "$logdir" "$dldir"

    if $SKIP_EXTRACT; then
        download_sources
        msg "Skipping source extraction"
    else
        cleanup_sources
        download_sources
        extract_sources
    fi
    pushd "$srcdir" &>/dev/null
    build
    popd &>/dev/null
fi

mkdir -p "$outdir/.pkgs"
echo -n "$pkgver-$pkgrel" >"$outdir/.pkgs/$pkgname"

checkdepends
finalise

if [[ $ONSCRLIB_INVALID -eq 1 && "$pkgname" == "onscrlib" ]]; then
    export ONSCRLIB_INVALID=0
    msg "There are some packages which need recompilation"
    msg "Running onscrlib building process one more time"
    ret=0
    $buildpkg ${ORIG_ARGS[@]} "onscrlib" || ret=$?
    # Error message will be pushed outside
    if (( $ret )); then
        exit 1
    fi
elif [[ $ONSCRLIB_INVALID -eq 1 && $PKGBUILD_RECURSION_DEPTH -eq 1 ]]; then
    export ONSCRLIB_INVALID=0
    warn "There are some packages which need recompilation, run"
    warn "$buildpkg -l"
    warn "for more info"
fi

msg_stop "Done with %s" "$pkgname"


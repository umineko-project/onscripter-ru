#!/bin/bash

#
# quickdroid.tool
# ONScripter-RU
#
# Drood multiarch generator.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

srcdir=$(dirname "$0")
pushd "$srcdir" &>/dev/null
srcdir="$(pwd)"
popd &>/dev/null

source "$srcdir/../Dependencies/common.sh" || exit 1
cd "$srcdir/../" || exit 1

EXTRA_ARGS="--droid-build"
EXTRA_MARGS=""

if [ "$1" == "--release" ]; then
  EXTRA_ARGS="$EXTRA_ARGS --release-build --strip-binary"
elif [ "$1" == "--debug" ]; then
  EXTRA_MARGS="DEBUG=1"
fi

archnames=(
  "arm"
  "arm64"
  "x86"
)

for arch in "${archnames[@]}"; do
  msg "Working with architecture $arch"
  
  msg2 "Configuring..."

  chmod +x configure
  ./configure $EXTRA_ARGS --droid-arch="$arch"
  if (( $? )); then
    error "Configuration failed"
    exit 1
  fi

  msg2 "Making..."
  make clean
  if (( $? )); then
    error "Make clean failed"
    exit 1
  fi

  make $EXTRA_MARGS
  if (( $? )); then
    error "Make failed"
    exit 1
  fi
  
  msg "Done working with $arch!"
done

msg "Getting you an apk..."

make apkall
if (( $? )); then
  error "Make apkall failed"
  exit 1
fi

msg "Done!"

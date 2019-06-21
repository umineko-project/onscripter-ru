#!/bin/bash

#
# onscrlib.sh
# ONScripter-RU
#
# Xcode dependency generation script.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

IOS=0

if [ ! -d "${PROJECT_DIR}/Dependencies" ]; then
  echo "Invalid path project path: ${PROJECT_DIR}!"
  exit 1
fi

if [ "${1}" == "x86_64h" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib64h"
  BLD_ARCH="x86_64"
  VERMIN="10.7"
elif [ "${1}" == "x86_64" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib64"
  BLD_ARCH="x86_64"
  VERMIN="10.6"
elif [ "${1}" == "i386" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib32"
  BLD_ARCH="i386"
  VERMIN="10.6"
elif [ "${1}" == "armv7" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib-armv7"
  BLD_ARCH="armv7"
  IOS=1
elif [ "${1}" == "armv7s" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib-armv7s"
  BLD_ARCH="armv7s"
  IOS=1
elif [ "${1}" == "arm64" ]; then
  DST="${PROJECT_DIR}/DerivedData/Xcode/onscrlib-arm64"
  BLD_ARCH="arm64"
  IOS=1
fi

mkdir -p "${DST}"

if [ "${2}" == "clean" ]; then
  echo "NOT cleaning onscrlib, do that manually!"
  exit 0
fi

pushd "${PROJECT_DIR}/Dependencies";
find . -type d -exec mkdir -p ${DST}/{} \;
find . -type f -exec cp {} ${DST}/{} \;
find . -type l -exec cp -a {} ${DST}/{} \;
popd

export PATH="/opt/local/bin:/opt/local/sbin:/usr/local/bin/:/usr/local/sbin/:$PATH"

# Fixes bz2 unpack issues
function tar() {
  /usr/bin/tar "$@"
}
export -f tar

pushd ${DST};
chmod +x build.sh

if (( $IOS )); then
  # Do NOT mess with our SDKs
  unset SDKROOT
  unset IPHONEOS_DEPLOYMENT_TARGET
  unset MACOSX_DEPLOYMENT_TARGET

  ret=0
  ./build.sh -i -a ${BLD_ARCH} onscrlib || ret=1
else
  # Fixes libcxx-abi / libcxx compilation
  export SDKROOT=""

  ret=0
  if [ "${1}" == "x86_64h" ]; then
    ./build.sh -a ${BLD_ARCH} -m ${VERMIN} onscrlib || ret=1
  else
    ./build.sh -a ${BLD_ARCH} -m ${VERMIN} onscrlib && ./build.sh -a ${BLD_ARCH} libcxx || ret=1
  fi
fi

if (( $ret )); then
  exit 1
fi

popd

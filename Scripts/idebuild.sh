#!/bin/bash

#
# idebuild.sh
# ONScripter-RU
#
# A hack script to allow building from an external IDE.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

# Usage:
# ./idebuild.sh action build_dir msys_dir bin_dir
# where actions include build, clean, debug

if [ "$3" != "" ]; then
  rm -f "$3/onscripter-ru"
fi

B_DEBUG=""
B_ACTION="build"
B_PROJECT_PATH="$2"

if [ "$1" == "debug" ]; then
  B_DEBUG="DEBUG=1"
fi

if [ "$1" == "clean" ]; then
  B_ACTION="clean"
fi

if [[ "$3" == *Debug* ]] || [[ "$3" == *debug* ]]; then
  B_DEBUG="DEBUG=1"
fi

MAKEFLAGS=""

if [ "$B_ACTION" == "clean" ]; then
  make -C "$B_PROJECT_PATH" clean || exit 1
else
  make -C "$B_PROJECT_PATH" $B_DEBUG -j 4 || exit 1
fi

if [ "$3" != "" ]; then
  cp "$B_PROJECT_PATH/onscripter-ru" "$3/onscripter-ru"
fi

exit 0

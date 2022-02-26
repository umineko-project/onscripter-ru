#!/bin/bash

#
# resources.sh
# ONScripter-RU
#
# Resource list generation, common among Xcode and configure.
# WARN: Xcode uses hints with absolute filenames in project settings.
#       This hints are used to determine rebuild status.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

RESOURCE_FILES=()
RESOURCE_PATHS=()
RESOURCE_NAMES=()
RESOURCE_LIST=()

if [ "${PROJECT_DIR}" != "" ]; then 
  RESOURCE_PREFIX="${PROJECT_DIR}/"
else
  RESOURCE_PREFIX=""
fi

for dep in "${RESOURCE_PREFIX}Resources/Shaders"/*.frag ; do
  RESOURCE_FILES+=("$dep")
  dep=${dep/.gles./.}
  dep=${dep/.gl./.}
  RESOURCE_PATHS+=("$dep")
done

for dep in "${RESOURCE_PREFIX}Resources/Shaders"/*.vert ; do
  RESOURCE_FILES+=("$dep")
  dep=${dep/.gles./.}
  dep=${dep/.gl./.}
  RESOURCE_PATHS+=("$dep")
done

for dep in "${RESOURCE_PREFIX}Resources/Images"/*.png ; do
  RESOURCE_FILES+=("$dep")
  RESOURCE_PATHS+=("$dep")
done

RESOURCE_PATHS=($(echo "${RESOURCE_PATHS[@]}" | tr ' ' '\n' | sort -u | tr '\n' ' '))

for dep in "${RESOURCE_PATHS[@]}"; do
  name=$(basename "$dep")
  RESOURCE_NAMES+=("$name")
  RESOURCE_LIST+=("${LIST_PREFIX}$dep" "$name")
done

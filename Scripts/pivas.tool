#!/bin/bash

#
# pivas.tool
# ONScripter-RU
#
# Pivas launcher script.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

cd "$(dirname "$0")/../" || exit 1

if [ -z ${EXCLUDE+x} ]; then
  # V001 - appears tp be just borked.
  # V668 - testing new result against null is sane in XNU kernel
  # V524 - is pretty broken and does not even respect enums
  # Conflicts with -fmodules!!!
  EXCLUDE="V001"
fi

# Use ALL to enable all, semi-colon separation is BROKEN.
if [ -z ${ANALYZERS+x} ]; then
  ANALYZERS="GA:1,2"
fi

rm -rf pvslog.txt pvslog compile_commands.json build

xcodebuild -target onscripter-ru-osx64h -configuration Debug | xcpretty --report json-compilation-database --output compile_commands.json || exit 1

pvs-studio-analyzer analyze -l ~/.config/PVS-Studio/PVS-Studio.lic  -o pvslog.txt -j8 || exit 1
plog-converter -a "$ANALYZERS" -t fullhtml -d "$EXCLUDE" -o pvslog pvslog.txt || exit 1

rm -rf pvslog.txt compile_commands.json build

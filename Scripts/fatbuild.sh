#!/bin/bash

#
# fatbuild.sh
# ONScripter-RU
#
# macOS FAT file generation (embeds multiple architectures).
# Run with "i386/executable" "x86_64/executable" "x86_64h/executable" "target/executable" arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

if (( $# < 5 )); then
  echo "Usage: i386/executable x86_64/executable x86_64h/executable target/executable"
  exit 1
fi

executable32="${1}"
executable64="${2}"
executable64h="${3}"
executabledst="${4}"
action="${5}"

if [ "$action" == "clean" ]; then
	exit 0
fi

echo "EXE32:  ${executable32}"
echo "EXE64:  ${executable64}"
echo "EXE64h: ${executable64h}"
echo "DST:    ${executabledst}"

if [ ! -x "${executable32}" ] || [ ! -x "${executable64}" ] || [ ! -x "${executable64h}" ] || [ ! -x "${executabledst}" ]; then
  echo "Missing dependent app for merging!"
  exit 1
fi

rm -rf /tmp/onscripter-ru-exec
mkdir -p /tmp/onscripter-ru-exec || exit 1

cp "${executable32}" /tmp/onscripter-ru-exec/ons32
cp "${executable64}" /tmp/onscripter-ru-exec/ons64
cp "${executable64h}" /tmp/onscripter-ru-exec/ons64h

# Set cpu_subtype to Haswell (until Xcode supports compiling for x86_64h)
echo -n -e "\x08\x00\x00\x00" | dd of="/tmp/onscripter-ru-exec/ons64h" bs=1 seek=8 count=4 conv=notrunc

# Create fat archive with i386, x86_64, x86_64h order, so that older os ignore x86_64h
lipo -create "/tmp/onscripter-ru-exec/ons32" "/tmp/onscripter-ru-exec/ons64" \
			"/tmp/onscripter-ru-exec/ons64h" -output "${executabledst}" || exit 1

rm -rf /tmp/onscripter-ru-exec

exit 0

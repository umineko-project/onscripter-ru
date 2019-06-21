#!/bin/bash

#
# ipabuild.tool
# ONScripter-RU
#
# iOS ipa generation script.
# Run with "ceritificate" "dir/with/apps" arguments.
#
# Consult LICENSE file for licensing terms and copyright holders.
#

scripts_dir=$(dirname "$0")

pushd "$scripts_dir" &>/dev/null
scripts_dir=$(pwd)
popd &>/dev/null

bundle_dir="$scripts_dir/../Resources/Bundle"

if [ "$1" != "" ]; then
  SIGNCERT="$1"
else
  SIGNCERT="RosatriceTheGolden"
fi

if [ "$2" != "" ]; then
  APPDIR="$2"
else
  APPDIR=$scripts_dir/../DerivedData/ONScripter/Build/Products/Release
fi

if [ ! -d "$bundle_dir" ]; then
  echo "No bundle directory"
  exit 1
fi

if [ ! -d "$APPDIR" ]; then
  echo "No release directory"
  exit 1
fi

pushd "$APPDIR" &>/dev/null
APPDIR=$(pwd)

for f in *ios*app ; do
  dir=${f/.app/}
  rm -rf $dir
  rm -f ${dir}.ipa
  mkdir -p $dir/Payload
  cp $bundle_dir/iTunesArtwork $dir
  cp -a $f $dir/Payload
  rm -f $dir/Payload/$f/embedded.mobileprovision
  touch $dir/Payload/$f/embedded.mobileprovision
  codesign --deep -f -s "$SIGNCERT" $dir/Payload/$f
  pushd $dir &>/dev/null
  zip -qry ../${dir}.ipa *
  popd &>/dev/null
done

popd &>/dev/null

exit 0

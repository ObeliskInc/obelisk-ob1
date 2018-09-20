#!/bin/bash

fromVer=`cat targetVersion`
fullVersion=`cat newVersion`
model=$(echo "$fullVersion" | awk '{print tolower($1)}')
toVer=$(echo "$fullVersion" | awk '{print $2}')
tarFilename="../../releases/$model-$fromVer-to-$toVer.tar.gz"

echo Building $tarFilename

mkdir -p ../../releases
rm -f $tarFilename
tar --exclude .DS_Store -czvf $tarFilename .
#!/bin/bash

d1=`dirname \`pwd\``
model=`basename ${d1}`
versions=`eval "cd ..;echo ${PWD##*/};cd - > /dev/null"`
tarFilename="../../releases/$model-$versions.tar.gz"

echo Building $tarFilename

mkdir -p ../../releases
echo $tarFilename
rm -f $tarFilename
tar --exclude .DS_Store -czvf $tarFilename .

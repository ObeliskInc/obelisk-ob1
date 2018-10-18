#!/bin/bash
./optimize-build.sh

SC1PATH="/Users/kencr2000/dev/obelisk-ob1/src/upgrades/sc1/v1.1.0_to_v1.2.0/rootfs-overlay/var/www"
DCR1PATH="/Users/kencr2000/dev/obelisk-ob1/src/upgrades/dcr1/v1.1.0_to_v1.2.0/rootfs-overlay/var/www"

set -x

rm -rf $SC1PATH
rm -rf $DCR1PATH
mkdir -p $SC1PATH
mkdir -p $DCR1PATH
cp -R build/* $SC1PATH
cp -R build/* $DCR1PATH

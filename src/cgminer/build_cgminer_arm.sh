#!/bin/bash

#A_OPENWRT_DIR=/home/pepe/openwrt_buildroot/openwrt
A_TARGET=arm
# export STAGING_DIR="${A_OPENWRT_DIR}/staging_dir"

# NOTE: Move this export into your ~/.bashrc file so each user can have their own path
# export TOOLCHAIN_PATH=/home/osboxes/tools/host
export PATH=$PATH:$TOOLCHAIN_PATH/bin/
export TOOL_PREFIX=arm-linux
export AR=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-ar
export AS=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-as
export LD=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-ld
export NM=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-nm
export CC=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-gcc
export CPP=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-cpp
export GCC=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-gcc
export CXX=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-g++
export RANLIB=$TOOLCHAIN_PATH/bin/$TOOL_PREFIX-ranlib

./configure --enable-obelisk --host=arm --disable-libcurl

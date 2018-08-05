#!/bin/bash
g++ --std=c++14 \
 src/test/testAuth.cpp \
 src/test/testSystem.cpp \
 src/utils/utils.cpp \
 src/utils/CgMinerUtils.cpp \
 -lcrypt \
 -lboost_system \
 -o ./bin/test
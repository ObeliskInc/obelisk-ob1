#!/bin/bash
g++ --std=c++14 \
 src/main.cpp \
 src/utils/utils.cpp \
 src/utils/base64.cpp \
 src/CrowMain.cpp \
 src/CgMinerMain.cpp \
 src/utils/CgMinerUtils.cpp \
 src/handlers/Handlers.cpp \
 -lboost_system \
 -lpthread \
 -o ./bin/apiserver
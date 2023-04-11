#! /bin/sh

BUILD_TYPE="Release"
#BUILD_TYPE="Debug"
#BUILD_TYPE="MinSizeRel"

DIR=$PWD
BUILD_DIR="build"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ -f "Makefile" ]; then
   cmake --build . --config "${BUILD_TYPE}"
else
   cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -G "MinGW Makefiles" ../
   cmake --build . --config "${BUILD_TYPE}"
fi


cd $DIR

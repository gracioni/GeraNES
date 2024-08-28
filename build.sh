#! /bin/sh

USE_MINGW=false

BUILD_TYPE="Release"
#BUILD_TYPE="Debug"
#BUILD_TYPE="MinSizeRel"

DIR=$PWD
BUILD_DIR="build"
mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [ -f "Makefile" ]; then
   cmake --build . --config "${BUILD_TYPE}"
   cp -rf ../data/* ./
else
   if [ "$USE_MINGW" = true ]; then
       cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -G "MinGW Makefiles" ../
   else
       cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ../
   fi
   cmake --build . --config "${BUILD_TYPE}"
   cp -rf ../data/* ./
fi


cd $DIR

#!/bin/bash

BUILD_DIRECTORY="build-release"
INSTALL_PREFIX=$HOME/.local
CACHE_BUILD=0
# Parse arguments
for arg in "$@"; do
  case $arg in
    --cache)
      CACHE_BUILD=1
      shift # Remove argument from the list
      ;;
    *)
      ;;
  esac
done

# Optionally remove the build directory
if [ $CACHE_BUILD -eq 0 ]; then
  rm -rf $BUILD_DIRECTORY
fi

mkdir -p $BUILD_DIRECTORY
cd $BUILD_DIRECTORY
cmake -DCMAKE_BUILD_TYPE=Release -DFISHNET_TEST=OFF -DFISHNET_APPS=ON -DFISHNET_TEST_LIB=OFF -DCMAKE_INSTALL_LIBDIR=lib -DFISHNET_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..
cmake --build . --config Release -j 16
cmake --install .
cd ..
# Optionally clean up the build directory
if [ $CACHE_BUILD -eq 0 ]; then
  rm -rf $BUILD_DIRECTORY
fi
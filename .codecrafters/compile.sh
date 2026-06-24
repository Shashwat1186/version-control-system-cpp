#!/bin/sh
#
# This script is used to compile your program on CodeCrafters
#
# This runs before .codecrafters/run.sh
#
# Learn more: https://codecrafters.io/program-interface

set -e # Exit on failure

cmake -G "MinGW Makefiles" -B build -S . -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DOPENSSL_ROOT_DIR=C:/msys64/ucrt64 -DZLIB_ROOT=C:/msys64/ucrt64
cmake --build ./build

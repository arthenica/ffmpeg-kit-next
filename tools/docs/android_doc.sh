#!/bin/bash
#
# Generates docs for Android Native library
#

CURRENT_DIR="`pwd`"

cd "${CURRENT_DIR}"/../../android/ffmpeg-kit-next-android-lib

mkdir -p "${CURRENT_DIR}"/../../docs/android/doc

doxygen

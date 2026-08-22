#!/bin/bash
#
# Generates javadocs for Android Java library
#

CURRENT_DIR="`pwd`"

cd "${CURRENT_DIR}"/../../android || exit

./gradlew -b ffmpeg-kit-next-android-lib/build.gradle dokkaJavadoc

rm -rf "${CURRENT_DIR}"/../../docs/android/javadoc

cp -r ffmpeg-kit-next-android-lib/build/dokka/javadoc "${CURRENT_DIR}"/../../docs/android/javadoc

cd "${CURRENT_DIR}" || exit
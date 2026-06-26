#!/bin/bash

rm -rf ../prebuilt
rm -rf ../.tmp
rm -f ../build.log
rm -rf ../.gradle

rm -rf ../android/build
rm -rf ../android/ffmpeg-kit-next-android-lib/build
rm -rf ../android/obj
rm -rf ../android/libs

rm -rf ../src/*

rm -rf ../apple/src/.deps
rm -rf ../apple/src/.libs

rm -rf ../linux/src/.deps
rm -rf ../linux/src/.libs

rm -rf ../react-native/android/libs-maven
rm -rf ../react-native/ios/Frameworks

rm -rf ../flutter/flutter/android/libs-maven
rm -rf ../flutter/flutter/ios/ffmpeg_kit_next_flutter/Frameworks
rm -rf ../flutter/flutter/macos/ffmpeg_kit_next_flutter/Frameworks
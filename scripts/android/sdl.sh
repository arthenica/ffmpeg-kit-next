#!/bin/bash

# SET BUILD OPTIONS
SDL_CMAKE_OPTIONS=("-DANDROID_ABI=$(get_android_cmake_ndk_abi)")
case ${ARCH} in
arm-v7a) SDL_CMAKE_OPTIONS+=("-DSDL_ARMNEON=OFF") ;;
arm-v7a-neon) SDL_CMAKE_OPTIONS+=("-DANDROID_ARM_NEON=ON" "-DSDL_ARMNEON=ON") ;;
esac

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

cmake -Wno-dev \
  -S "${BASEDIR}/src/${LIB_NAME}" \
  -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
  -DANDROID_PLATFORM="android-${API}" \
  -DANDROID_STL=none \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DSDL_SHARED=OFF \
  -DSDL_STATIC=ON \
  -DSDL_INSTALL=ON \
  -DSDL_INSTALL_CPACK=OFF \
  -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF \
  -DSDL_ANDROID_JAR=OFF \
  -DSDL_AUDIO=ON \
  -DSDL_VIDEO=ON \
  -DSDL_RENDER=ON \
  -DSDL_HIDAPI=OFF \
  -DSDL_SENSOR=OFF \
  -DSDL_CAMERA=OFF \
  -DSDL_DIALOG=OFF \
  -DSDL_DISKAUDIO=OFF \
  "${SDL_CMAKE_OPTIONS[@]}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

mkdir -p "${LIB_INSTALL_PREFIX}/share/sdl3/java" || return 1
cp -R "${BASEDIR}/src/${LIB_NAME}/android-project/app/src/main/java/org" \
  "${LIB_INSTALL_PREFIX}/share/sdl3/java/" || return 1
cp "${LIB_INSTALL_PREFIX}/lib/pkgconfig/sdl3.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1

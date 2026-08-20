#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
git clean -dfx 2>/dev/null 1>/dev/null

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

ANDROID_NDK_OPTIONS="-DANDROID_ABI=$(get_android_cmake_ndk_abi)"
case ${ARCH} in
arm-v7a-neon)
  ANDROID_NDK_OPTIONS="${ANDROID_NDK_OPTIONS} -DANDROID_ARM_NEON=TRUE"
  ;;
esac

cmake -Wno-dev \
 -DUSE_ENCLIB=openssl \
 -DCMAKE_VERBOSE_MAKEFILE=0 \
 -DCMAKE_C_FLAGS="${CFLAGS}" \
 -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
 -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
 -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
 -DCMAKE_MAKE_PROGRAM="$(command -v make)" \
 -DANDROID_PLATFORM=android-${API} \
 ${ANDROID_NDK_OPTIONS} \
 -DENABLE_STDCXX_SYNC=1 \
 -DENABLE_MONOTONIC_CLOCK=1 \
 -DENABLE_CXX11=1 \
 -DUSE_OPENSSL_PC=1 \
 -DENABLE_DEBUG=0 \
 -DENABLE_LOGGING=0 \
 -DENABLE_HEAVY_LOGGING=0 \
 -DENABLE_APPS=0 \
 -DENABLE_SHARED=0 "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_srt_package_config "1.5.5" || return 1

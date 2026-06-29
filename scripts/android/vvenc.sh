#!/bin/bash

# SET BUILD OPTIONS
ARM_SIMD_OPTIONS=""
ANDROID_NDK_OPTIONS="-DANDROID_ABI=$(get_android_cmake_ndk_abi)"
case ${ARCH} in
arm-v7a)
  ARM_SIMD_OPTIONS="-DVVENC_ENABLE_ARM_SIMD=OFF"
  ;;
arm-v7a-neon)
  ANDROID_NDK_OPTIONS="${ANDROID_NDK_OPTIONS} -DANDROID_ARM_NEON=TRUE"
  ARM_SIMD_OPTIONS="-DVVENC_ENABLE_ARM_SIMD=ON"
  ;;
arm64-v8a)
  ARM_SIMD_OPTIONS="-DVVENC_ENABLE_ARM_SIMD=ON"
  ;;
esac

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_ROOT}/build/cmake/android.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DANDROID_PLATFORM=android-${API} \
  -DANDROID_STL=c++_shared \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DBUILD_SHARED_LIBS=0 \
  -DVVENC_LIBRARY_ONLY=1 \
  -DVVENC_ENABLE_INSTALL=1 \
  -DVVENC_ENABLE_LINK_TIME_OPT=0 \
  -DVVENC_ENABLE_WERROR=0 \
  -DVVENC_ENABLE_ARM_SIMD_SVE=0 \
  -DVVENC_ENABLE_ARM_SIMD_SVE2=0 \
  -DVVENC_ENABLE_THIRDPARTY_JSON=0 \
  -DVVENC_TOPLEVEL_OUTPUT_DIRS=0 \
  ${ANDROID_NDK_OPTIONS} \
  ${ARM_SIMD_OPTIONS} \
  "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

if [[ -f "${LIB_INSTALL_PREFIX}/lib/pkgconfig/libvvenc.pc" ]]; then
  cp "${LIB_INSTALL_PREFIX}/lib/pkgconfig/libvvenc.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1
elif [[ -f "${LIB_INSTALL_PREFIX}/lib64/pkgconfig/libvvenc.pc" ]]; then
  cp "${LIB_INSTALL_PREFIX}/lib64/pkgconfig/libvvenc.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1
else
  return 1
fi

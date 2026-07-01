#!/bin/bash

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_SYSROOT="${SDK_PATH}" \
  -DCMAKE_FIND_ROOT_PATH="${SDK_PATH}" \
  -DCMAKE_OSX_SYSROOT="$(get_sdk_name)" \
  -DCMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)" \
  -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_LINKER="$LD" \
  -DCMAKE_AR="$(xcrun --sdk $(get_sdk_name) -f ar)" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_target_cpu)" \
  -DBUILD_SHARED_LIBS=0 \
  -DBUILD_APPS=0 \
  -DBUILD_TESTING=0 \
  -DCOMPILE_C_ONLY=1 \
  -DSVT_AV1_LTO=0 \
  -DNATIVE=0 \
  -DENABLE_SVE=0 \
  -DENABLE_SVE2=0 \
  -DENABLE_NEON_DOTPROD=0 \
  -DENABLE_NEON_I8MM=0 \
  -DREPRODUCIBLE_BUILDS=1 \
  "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

if [[ -f "${LIB_INSTALL_PREFIX}/lib/pkgconfig/SvtAv1Enc.pc" ]]; then
  cp "${LIB_INSTALL_PREFIX}/lib/pkgconfig/SvtAv1Enc.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1
elif [[ -f "${LIB_INSTALL_PREFIX}/lib64/pkgconfig/SvtAv1Enc.pc" ]]; then
  cp "${LIB_INSTALL_PREFIX}/lib64/pkgconfig/SvtAv1Enc.pc" "${INSTALL_PKG_CONFIG_DIR}" || return 1
else
  return 1
fi

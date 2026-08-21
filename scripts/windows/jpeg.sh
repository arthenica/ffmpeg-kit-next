#!/bin/bash

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_AR="$AR" \
  -DCMAKE_RC_COMPILER="$WINDRES" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DENABLE_STATIC=1 \
  -DENABLE_SHARED=0 \
  -DWITH_JPEG8=1 \
  -DWITH_SIMD=1 \
  -DREQUIRE_SIMD=1 \
  -DWITH_TURBOJPEG=0 \
  -DWITH_JAVA=0 \
  -DCMAKE_SYSTEM_PROCESSOR=$(get_cmake_system_processor) \
  "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path "${BUILD_DIR}"/pkgscripts/libjpeg.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

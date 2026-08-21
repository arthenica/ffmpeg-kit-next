#!/bin/bash

mkdir -p "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emcmake cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DEMSCRIPTEN_SYSTEM_PROCESSOR="$(get_cmake_system_processor)" \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_cmake_system_processor)" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DBUILD_SHARED_LIBS=0 \
  -DVVENC_LIBRARY_ONLY=1 \
  -DVVENC_ENABLE_INSTALL=1 \
  -DVVENC_ENABLE_LINK_TIME_OPT=0 \
  -DVVENC_ENABLE_WERROR=0 \
  -DVVENC_ENABLE_X86_SIMD=0 \
  -DVVENC_ENABLE_ARM_SIMD=0 \
  -DVVENC_ENABLE_ARM_SIMD_SVE=0 \
  -DVVENC_ENABLE_ARM_SIMD_SVE2=0 \
  -DVVENC_ENABLE_THIRDPARTY_JSON=0 \
  -DVVENC_TOPLEVEL_OUTPUT_DIRS=0 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${LIB_INSTALL_PREFIX}"/lib/pkgconfig/libvvenc.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

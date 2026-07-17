#!/bin/bash

mkdir -p "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# WORKAROUND TO DETECT ASM FLAGS PROPERLY
${SED_INLINE} 's/ ${CPUINFO}/ "${CPUINFO}"/g' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeModules/FindSSE.cmake 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# vid.stab declares cmake_minimum_required(VERSION 2.8.5);
# CMake 4.x removed compatibility with policy versions below 3.5.
emcmake cmake -Wno-dev \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DUSE_OMP=0 \
  -DSSE2_FOUND=0 \
  -DSSE3_FOUND=0 \
  -DSSSE3_FOUND=0 \
  -DSSE4_1_FOUND=0 \
  -DBUILD_SHARED_LIBS=0 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp vidstab.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

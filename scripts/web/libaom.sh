#!/bin/bash

mkdir -p "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emcmake cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCONFIG_PIC=1 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DAOM_TARGET_CPU=generic \
  -DENABLE_TESTS=0 \
  -DENABLE_EXAMPLES=0 \
  -DENABLE_TOOLS=0 \
  -DENABLE_DOCS=0 \
  -DCONFIG_UNIT_TESTS=0 \
  -DBUILD_SHARED_LIBS=0 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${LIB_INSTALL_PREFIX}"/lib/pkgconfig/aom.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

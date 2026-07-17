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
  -DBUILD_APPS=0 \
  -DBUILD_TESTING=0 \
  -DCOMPILE_C_ONLY=1 \
  -DSVT_AV1_LTO=0 \
  -DNATIVE=0 \
  -DREPRODUCIBLE_BUILDS=1 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${LIB_INSTALL_PREFIX}"/lib/pkgconfig/SvtAv1Enc.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

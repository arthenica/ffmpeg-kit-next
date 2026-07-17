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
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DSNAPPY_BUILD_TESTS=0 \
  -DSNAPPY_BUILD_BENCHMARKS=0 \
  -DHAVE_LIBLZO2=0 \
  -DHAVE_LIBZ=0 \
  -DBUILD_SHARED_LIBS=0 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_snappy_package_config "1.2.2" || return 1

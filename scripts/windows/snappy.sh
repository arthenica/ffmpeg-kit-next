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
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_AR="$AR" \
  -DCMAKE_RC_COMPILER="$WINDRES" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DSNAPPY_BUILD_TESTS=0 \
  -DSNAPPY_BUILD_BENCHMARKS=0 \
  -DHAVE_LIBLZO2=0 \
  -DHAVE_LIBZ=0 \
  -DCMAKE_SYSTEM_PROCESSOR=$(get_cmake_system_processor) \
  -DBUILD_SHARED_LIBS=0 "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_snappy_package_config "1.2.2" || return 1

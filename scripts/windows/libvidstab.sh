#!/bin/bash

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  ASM_OPTIONS="-DSSE2_FOUND=0 -DSSE3_FOUND=0 -DSSSE3_FOUND=0 -DSSE4_1_FOUND=0"
  ;;
esac

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

# WORKAROUND TO DETECT ASM FLAGS PROPERLY
${SED_INLINE} 's/ ${CPUINFO}/ "${CPUINFO}"/g' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeModules/FindSSE.cmake 1>>"${BASEDIR}"/build.log 2>&1 || return 1

cmake -Wno-dev \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_AR="$AR" \
  -DCMAKE_RC_COMPILER="$WINDRES" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DUSE_OMP=0 \
  ${ASM_OPTIONS} \
  -DCMAKE_SYSTEM_PROCESSOR=$(get_cmake_system_processor) \
  -DBUILD_SHARED_LIBS=0 "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path vidstab.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

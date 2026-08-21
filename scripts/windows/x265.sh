#!/bin/bash

# SET BUILD OPTIONS
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/source/CMakeLists.txt || return 1
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  # ENABLE_ASSEMBLY stays on for the baseline NEON aarch64 primitives. SVE/SVE2/BitPerm
  # are armv9 extensions that are not assembled for our target, so they are disabled
  # below. CROSS_COMPILE_ARM64 forces x265 4.2 into its aarch64 code path.
  ASM_OPTIONS="-DENABLE_ASSEMBLY=1 -DCROSS_COMPILE_ARM64=1 -DENABLE_SVE=0 -DENABLE_SVE2=0 -DENABLE_SVE2_BITPERM=0"
  # x265 4.2 rewrote the aarch64 ARM_ARGS to `set(ARM_ARGS -O3)`; inject the clang
  # target here so it reaches both add_definitions(${ARM_ARGS}) and the aarch64 asm
  # custom command (which assembles via ${CMAKE_CXX_COMPILER} ${ARM_ARGS} ...).
  ${SED_INLINE} "s|set(ARM_ARGS -O3)|set(ARM_ARGS -O3 --target=$(get_clang_host))|g" "${BASEDIR}"/src/"${LIB_NAME}"/source/CMakeLists.txt || return 1
  ;;
x86-64)
  if ! [ -x "$(command -v nasm)" ]; then
    echo -e "\n(*) nasm command not found\n"
    return 1
  fi

  ASM_OPTIONS="-DENABLE_ASSEMBLY=1"
  ;;
esac

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

# WORKAROUND TO FIX static_assert ERRORS
${SED_INLINE} 's/gnu++98/c++11/g' "${BASEDIR}"/src/"${LIB_NAME}"/source/CMakeLists.txt || return 1

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
  -DENABLE_PIC=1 \
  -DENABLE_CLI=0 \
  ${ASM_OPTIONS} \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_cmake_system_processor)" \
  -DENABLE_SHARED=0 "${BASEDIR}"/src/"${LIB_NAME}"/source || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_x265_package_config "4.2" || return 1

#!/bin/bash

# SET BUILD FLAGS
export CCAS=${AS}
export ASM_FLAGS=$(get_asmflags "${LIB_NAME}")

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
armv7 | armv7s | arm64*)
  ASM_OPTIONS="-DWITH_SIMD=1"
  ;;
*)
  case ${FFMPEG_KIT_BUILD_TYPE} in
  ios)
    ASM_OPTIONS="-DWITH_SIMD=0"
    ;;
  *)
    ASM_OPTIONS="-DWITH_SIMD=1"
    ;;
  esac
  ;;
esac

# DETERMINE POINTER SIZE PER ARCH SO LIBJPEG-TURBO CAN DERIVE SIMD CPU_TYPE UNDER CMAKE_SYSTEM_NAME=Generic
case ${ARCH} in
i386 | armv7 | armv7s)
  SIMD_VOID_P_SIZE=4
  ;;
*)
  SIMD_VOID_P_SIZE=8
  ;;
esac

# libjpeg-turbo derives CPU_TYPE from CMAKE_OSX_ARCHITECTURES when it is set.
# arm64e matches its arm64 check, but is not a supported SIMD CPU_TYPE.  Leave
# the CMake arch list empty for arm64e and rely on CFLAGS/LDFLAGS for -arch.
case ${ARCH} in
arm64e)
  JPEG_CMAKE_OSX_ARCHITECTURES=""
  ;;
*)
  JPEG_CMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)"
  ;;
esac

# WORKAROUND TO FIX ASM FLAGS
${SED_INLINE} 's/${CMAKE_C_FLAGS} ${CMAKE_ASM_FLAGS}/${CMAKE_ASM_FLAGS}/g' "${BASEDIR}"/src/"${LIB_NAME}"/simd/CMakeLists.txt

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
  -DCMAKE_OSX_ARCHITECTURES="${JPEG_CMAKE_OSX_ARCHITECTURES}" \
  -DCMAKE_SYSTEM_NAME=Darwin \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_LINKER="$LD" \
  -DCMAKE_AR="$(xcrun --sdk $(get_sdk_name) -f ar)" \
  -DCMAKE_ASM_FLAGS="$ASM_FLAGS" \
  -DENABLE_PIC=1 \
  -DENABLE_STATIC=1 \
  -DENABLE_SHARED=0 \
  -DWITH_JPEG8=1 \
  ${ASM_OPTIONS} \
  -DREQUIRE_SIMD=1 \
  -DWITH_TURBOJPEG=0 \
  -DWITH_JAVA=0 \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_target_cpu)" \
  -DCMAKE_SIZEOF_VOID_P=${SIMD_VOID_P_SIZE} \
  -DBUILD_SHARED_LIBS=0 "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${BUILD_DIR}"/pkgscripts/libjpeg.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

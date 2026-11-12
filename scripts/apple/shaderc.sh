#!/bin/bash

SHADERC_SOURCE_DIR="${BASEDIR}/src/${LIB_NAME}"

# CLONE THE THIRD PARTY SOURCES FROM THE ARTHENICA FORKS AT THE COMMITS LISTED IN THE DEPS FILE OF SHADERC
if [[ $(clone_shaderc_third_party "${SHADERC_SOURCE_DIR}") -ne 0 ]]; then
  echo -e "ERROR: Failed to clone the third party sources of ${LIB_NAME}\n"
  return 1
fi

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
  -DCMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)" \
  -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
  -DCMAKE_MACOSX_BUNDLE=0 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_LINKER="$LD" \
  -DCMAKE_AR="$(xcrun --sdk $(get_sdk_name) -f ar)" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_target_cpu)" \
  -DBUILD_SHARED_LIBS=OFF \
  -DSHADERC_SKIP_TESTS=ON \
  -DSHADERC_SKIP_EXAMPLES=ON \
  -DSHADERC_SKIP_EXECUTABLES=ON \
  -DSHADERC_SKIP_COPYRIGHT_CHECK=ON \
  -DENABLE_GLSLANG_BINARIES=OFF \
  -DSPIRV_SKIP_EXECUTABLES=ON \
  -DSPIRV_SKIP_TESTS=ON \
  "${SHADERC_SOURCE_DIR}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

if [[ -d "${LIB_INSTALL_PREFIX}/lib/pkgconfig" ]]; then
  SHADERC_LIB_DIR="${LIB_INSTALL_PREFIX}/lib"
elif [[ -d "${LIB_INSTALL_PREFIX}/lib64/pkgconfig" ]]; then
  SHADERC_LIB_DIR="${LIB_INSTALL_PREFIX}/lib64"
else
  echo -e "ERROR: pkgconfig directory not found under ${LIB_INSTALL_PREFIX}/lib or ${LIB_INSTALL_PREFIX}/lib64\n"
  return 1
fi

# THE STATIC BUNDLE OF SHADERC, GLSLANG AND SPIRV-TOOLS IS THE ONLY SHADERC LIBRARY USED
if [[ ! -f "${SHADERC_LIB_DIR}/libshaderc_combined.a" ]]; then
  echo -e "ERROR: libshaderc_combined.a not found under ${SHADERC_LIB_DIR}\n"
  return 1
fi

# libplacebo LOOKS FOR shaderc, WHICH SHADERC INSTALLS FOR ITS SHARED LIBRARY. DELETE THE SHARED LIBRARY AND ITS
# shaderc.pc, SO NOTHING CAN LINK IT
rm -f "${SHADERC_LIB_DIR}"/libshaderc_shared.* "${SHADERC_LIB_DIR}/pkgconfig/shaderc.pc" || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_shaderc_package_config "2026.4.1" "${SHADERC_LIB_DIR##*/}" || return 1

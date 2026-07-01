#!/bin/bash

# INIT SUBMODULES
${SED_INLINE} 's|/abseil/|/arthenica/|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
git submodule update --init || return 1

# DO NOT BUILD THE iLBC TEST EXECUTABLES
# CMake links them as plain macOS binaries, which fails for Mac Catalyst with
# "building for macCatalyst, but linking in object file ... built for macOS".
# The library does not need them. Other CMake libraries pass -DENABLE_TESTS=0, but
# libilbc has no such option, so the test targets are patched out of CMakeLists.txt.
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt 1>>"${BASEDIR}"/build.log 2>&1
${SED_INLINE} '/^add_executable(ilbc_test /,/decoded2.pcm)/d' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt
${SED_INLINE} 's/install(TARGETS ilbc ilbc_test/install(TARGETS ilbc/' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt

mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

# NOTE: the compiler is passed below via CMAKE_C_COMPILER_ID / CMAKE_CXX_COMPILER_ID,
# NOT CMAKE_C_COMPILER / CMAKE_CXX_COMPILER. This is intentional: setting the compiler
# explicitly makes CMake's AppleClang platform handling override the Mac Catalyst
# (macabi) -target from CFLAGS, so the objects get built for macOS (Mach-O platform 1)
# instead of Mac Catalyst (platform 6) and fail to link into the Catalyst FFmpeg build
# ("building for macCatalyst, but linking in object file ... built for macOS"). Leaving
# the compiler auto-detected preserves the macabi target. Do not "fix" this to
# CMAKE_C_COMPILER without rebuilding and verifying the mac-catalyst target.
cmake -Wno-dev \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_ASM_FLAGS="${ASM_FLAGS}" \
  -DCMAKE_SYSROOT="${SDK_PATH}" \
  -DCMAKE_FIND_ROOT_PATH="${SDK_PATH}" \
  -DCMAKE_OSX_SYSROOT="$(get_sdk_name)" \
  -DCMAKE_OSX_ARCHITECTURES="$(get_cmake_osx_architectures)" \
  -DCMAKE_SYSTEM_NAME="Darwin" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_C_COMPILER_ID="$CC" \
  -DCMAKE_CXX_COMPILER_ID="$CXX" \
  -DCMAKE_LINKER="$LD" \
  -DCMAKE_AR="$(xcrun --sdk $(get_sdk_name) -f ar)" \
  -DCMAKE_AS="$AS" \
  -DCMAKE_SYSTEM_PROCESSOR="$(get_target_cpu)" \
  -DBUILD_SHARED_LIBS=0 "${BASEDIR}"/src/"${LIB_NAME}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${BUILD_DIR}"/libilbc.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

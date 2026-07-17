#!/bin/bash

# WEB (WASM) SRT BUILD
# CMake build via Emscripten. Links against the web OpenSSL build through
# pkg-config and disables apps/shared libraries/logging. Emscripten does not pass
# SRT's CLOCK_MONOTONIC probe, so the monotonic clock path stays off. Static + PIC.

OPENSSL_INSTALL_PREFIX="${LIB_INSTALL_BASE}/openssl"
OPENSSL_LIB_DIR="${OPENSSL_INSTALL_PREFIX}/lib"
OPENSSL_INCLUDE_DIR="${OPENSSL_INSTALL_PREFIX}/include"

# CMake's FindPkgConfig uses PKG_CONFIG_PATH, while emconfigure/emcmake also
# consult EM_PKG_CONFIG_PATH for Emscripten builds. Keep all three pointed at the
# web pkg-config directory so SRT can find openssl.pc/libcrypto.pc.
export PKG_CONFIG_LIBDIR="$(get_web_pkg_config_libdir)"
export PKG_CONFIG_PATH="$(get_web_pkg_config_libdir)"
export EM_PKG_CONFIG_PATH="$(get_web_pkg_config_libdir)"

# SRT 1.5.5 does not recognize CMAKE_SYSTEM_NAME=Emscripten. Add a dedicated
# platform shortcut/branch so the CMake platform check passes without routing
# wasm through Linux-only epoll/SO_BINDTODEVICE code.
if ! grep -Fq 'set_if(EMSCRIPTEN' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt; then
  ${SED_INLINE} '/set_if(LINUX/a set_if(EMSCRIPTEN ${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} 's|set_if(POSIX       LINUX OR DARWIN OR BSD OR SUNOS OR ANDROID OR OHOS OR (CYGWIN AND CYGWIN_USE_POSIX) OR GNU_OS)|set_if(POSIX       LINUX OR DARWIN OR BSD OR SUNOS OR ANDROID OR OHOS OR EMSCRIPTEN OR (CYGWIN AND CYGWIN_USE_POSIX) OR GNU_OS)|g' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} '/elseif(OHOS)/i elseif(EMSCRIPTEN)\
	message(STATUS "DETECTED SYSTEM: EMSCRIPTEN" )' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi
${SED_INLINE} '/elseif(EMSCRIPTEN)/,/elseif(OHOS)/{/add_definitions(-DLINUX=1)/d; s/message(STATUS "DETECTED SYSTEM: EMSCRIPTEN;  LINUX=1" )/message(STATUS "DETECTED SYSTEM: EMSCRIPTEN" )/;}' "${BASEDIR}"/src/"${LIB_NAME}"/CMakeLists.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# Emscripten does not match SRT's endian platform list. WebAssembly is
# little-endian, and Emscripten provides the normal htons/htonl helpers.
if ! grep -Fq 'defined(__EMSCRIPTEN__)' "${BASEDIR}"/src/"${LIB_NAME}"/srtcore/utilities.h; then
  ${SED_INLINE} '/#elif defined(__WINDOWS__)/i #elif defined(__EMSCRIPTEN__)\
\
#	include <arpa/inet.h>\
\
#	define htobe16(x) htons(x)\
#	define htole16(x) (x)\
#	define be16toh(x) ntohs(x)\
#	define le16toh(x) (x)\
\
#	define htobe32(x) htonl(x)\
#	define htole32(x) (x)\
#	define be32toh(x) ntohl(x)\
#	define le32toh(x) (x)\
\
#	define htobe64(x) __builtin_bswap64(x)\
#	define htole64(x) (x)\
#	define be64toh(x) __builtin_bswap64(x)\
#	define le64toh(x) (x)\
\
#	define __BIG_ENDIAN 4321\
#	define __LITTLE_ENDIAN 1234\
#	define __PDP_ENDIAN 3412\
#	define __BYTE_ORDER __LITTLE_ENDIAN\
' "${BASEDIR}"/src/"${LIB_NAME}"/srtcore/utilities.h 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

mkdir -p "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emcmake cmake -Wno-dev \
  -DUSE_ENCLIB=openssl \
  -DCMAKE_VERBOSE_MAKEFILE=0 \
  -DCMAKE_C_FLAGS="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS="${CXXFLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${LDFLAGS}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${LIB_INSTALL_PREFIX}" \
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
  -DCMAKE_PREFIX_PATH="${OPENSSL_INSTALL_PREFIX}" \
  -DOPENSSL_ROOT_DIR="${OPENSSL_INSTALL_PREFIX}" \
  -DOPENSSL_INCLUDE_DIR="${OPENSSL_INCLUDE_DIR}" \
  -DOPENSSL_CRYPTO_LIBRARY="${OPENSSL_LIB_DIR}/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="${OPENSSL_LIB_DIR}/libssl.a" \
  -DOPENSSL_USE_STATIC_LIBS=1 \
  -DSRT_USE_OPENSSL_STATIC_LIBS=1 \
  -DENABLE_STDCXX_SYNC=1 \
  -DENABLE_MONOTONIC_CLOCK=OFF \
  -DENABLE_CXX11=1 \
  -DUSE_OPENSSL_PC=1 \
  -DENABLE_DEBUG=0 \
  -DENABLE_LOGGING=0 \
  -DENABLE_HEAVY_LOGGING=0 \
  -DENABLE_APPS=0 \
  -DENABLE_SHARED=0 \
  -DBUILD_SHARED_LIBS=0 \
  "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_srt_package_config "1.5.5" || return 1

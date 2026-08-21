#!/bin/bash

# SET BUILD OPTIONS
#
# OpenSSL ONLY SHIPS mingw (x86) AND mingw64 (x86_64) TARGETS. THERE IS NO
# aarch64 MinGW TARGET UPSTREAM, SO ONE IS SUPPLIED THROUGH --config. SEE
# tools/patch/openssl/mingw64-aarch64.conf FOR WHAT IT DEFINES.
CONFIG_OPTIONS=""
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  CONFIG_OPTIONS="--config=${BASEDIR}/tools/patch/openssl/mingw64-aarch64.conf"
  ASM_OPTIONS="mingw64-aarch64"
  ;;
x86-64)
  ASM_OPTIONS="mingw64 enable-ec_nistp_64_gcc_128"
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_openssl} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

INT128_AVAILABLE=$($CC -dM -E - </dev/null 2>>"${BASEDIR}"/build.log | grep __SIZEOF_INT128__)

echo -e "INFO: __uint128_t detection output: $INT128_AVAILABLE\n" 1>>"${BASEDIR}"/build.log 2>&1

# --libdir=lib PINS THE INSTALL LAYOUT: WITHOUT IT THE mingw64 TARGET INSTALLS
# INTO lib64, WHICH WOULD DIFFER BETWEEN THE TWO ARCHITECTURES.
./Configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --libdir=lib \
  ${CONFIG_OPTIONS} \
  --with-zlib-include="${LIB_INSTALL_BASE}/zlib/include" \
  --with-zlib-lib="${LIB_INSTALL_BASE}/zlib/lib/libz.a" \
  zlib \
  no-shared \
  no-engine \
  no-dso \
  no-legacy \
  ${ASM_OPTIONS} \
  no-tests || return 1

make -j$(get_cpu_count) build_sw || return 1

make install_sw install_ssldirs || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./exporters/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

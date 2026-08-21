#!/bin/bash

# SET BUILD OPTIONS
if [[ -z ${NETTLE_CC_FOR_BUILD} ]]; then
  if [[ -x /usr/bin/cc ]]; then
    NETTLE_CC_FOR_BUILD="/usr/bin/cc"
  else
    NETTLE_CC_FOR_BUILD="$(command -v cc || command -v clang || command -v gcc)"
  fi
fi
if [[ -z ${NETTLE_CC_FOR_BUILD} ]]; then
  echo -e "\nERROR: Native C compiler not found for Nettle build-time generators\n" 1>>"${BASEDIR}"/build.log 2>&1
  return 1
fi

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_nettle} -eq 1 ]]; then

  # WORKAROUND TO FIX BUILD SYSTEM COMPILER DETECTION
  overwrite_file "${BASEDIR}"/tools/patch/make/nettle/aclocal.m4 "${BASEDIR}"/src/"${LIB_NAME}"/aclocal.m4 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

# UPDATE CONFIG FILES TO SUPPORT wasm32-unknown-emscripten
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.guess "${BASEDIR}"/src/"${LIB_NAME}"/config.guess 1>>"${BASEDIR}"/build.log 2>&1 || return 1
overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.sub "${BASEDIR}"/src/"${LIB_NAME}"/config.sub 1>>"${BASEDIR}"/build.log 2>&1 || return 1

if [[ -z ${NETTLE_CC_FOR_BUILD} ]]; then
  if [[ -x /usr/bin/cc ]]; then
    NETTLE_CC_FOR_BUILD="/usr/bin/cc"
  else
    NETTLE_CC_FOR_BUILD="$(command -v cc || command -v clang || command -v gcc)"
  fi
fi
if [[ -z ${NETTLE_CC_FOR_BUILD} ]]; then
  echo -e "\nERROR: Native C compiler not found for Nettle build-time generators\n" 1>>"${BASEDIR}"/build.log 2>&1
  return 1
fi

emconfigure env \
  CC_FOR_BUILD="${NETTLE_CC_FOR_BUILD}" \
  HOST_CC="${NETTLE_CC_FOR_BUILD}" \
  CPP_FOR_BUILD="${NETTLE_CC_FOR_BUILD} -E" \
  ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --enable-pic \
  --enable-static \
  --with-include-path="${LIB_INSTALL_BASE}"/gmp/include \
  --with-lib-path="${LIB_INSTALL_BASE}"/gmp/lib \
  --disable-shared \
  --disable-mini-gmp \
  --disable-assembler \
  --disable-openssl \
  --disable-gcov \
  --disable-documentation \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

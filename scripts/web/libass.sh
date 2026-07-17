#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libass} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --disable-libtool-lock \
  --enable-static \
  --disable-shared \
  --disable-require-system-font-provider \
  --disable-fast-install \
  --disable-test \
  --disable-profile \
  --disable-coretext \
  --disable-directwrite \
  --disable-libunibreak \
  --disable-asm \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

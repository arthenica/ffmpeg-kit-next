#!/bin/bash

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  ASM_OPTIONS="--enable-hardware-optimizations --enable-arm-neon=yes"
  ;;
x86-64)
  ASM_OPTIONS="--enable-hardware-optimizations --enable-intel-sse=yes"
  ;;
esac

# UPDATE BUILD FLAGS
export CPPFLAGS="${CPPFLAGS} $(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)" || return 1
export LDFLAGS="${LDFLAGS} $(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libpng} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-unversioned-libpng-pc \
  --disable-unversioned-libpng-config \
  ${ASM_OPTIONS} \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

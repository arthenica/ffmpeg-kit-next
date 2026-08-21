#!/bin/bash

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  ASM_OPTIONS="--enable-neon --enable-neon-rtcd"
  ;;
x86-64)
  ASM_OPTIONS="--enable-sse2 --enable-sse4.1"
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libwebp} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-dependency-tracking \
  --enable-libwebpmux \
  ${ASM_OPTIONS} \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ${BASEDIR}/src/${LIB_NAME}/src/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
copy_and_update_path ${BASEDIR}/src/${LIB_NAME}/src/demux/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
copy_and_update_path ${BASEDIR}/src/${LIB_NAME}/src/mux/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
copy_and_update_path ${BASEDIR}/src/${LIB_NAME}/sharpyuv/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

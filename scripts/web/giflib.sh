#!/bin/bash

# ADD S_IREAD/S_IWRITE ALIASES EXPECTED BY GIFLIB
export CFLAGS="$(get_cflags "${LIB_NAME}") -DS_IREAD=S_IRUSR -DS_IWRITE=S_IWUSR"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# COPY BUILD FILES
cp "${BASEDIR}"/tools/patch/make/giflib/* "${BASEDIR}"/src/"${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_giflib} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_giflib_package_config "5.2.2" || return 1

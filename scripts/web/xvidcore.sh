#!/bin/bash

cd "${BASEDIR}"/src/"${LIB_NAME}"/"${LIB_NAME}"/build/generic || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/"${LIB_NAME}"/build/generic/configure ]] || [[ ${RECONF_xvidcore} -eq 1 ]]; then
  ./bootstrap.sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --disable-assembly \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_xvidcore_package_config "1.3.7" || return 1

# WORKAROUND TO REMOVE DYNAMIC LIBS
rm -f "${LIB_INSTALL_PREFIX}"/lib/libxvidcore.so* 1>>"${BASEDIR}"/build.log 2>&1

#!/bin/bash

cd "${BASEDIR}"/src/"${LIB_NAME}"/"${LIB_NAME}"/build/generic || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# WORKAROUNDS
git checkout configure.in 1>>"${BASEDIR}"/build.log 2>&1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/"${LIB_NAME}"/build/generic/configure ]] || [[ ${RECONF_xvidcore} -eq 1 ]]; then
  ./bootstrap.sh
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --host="${HOST}" || return 1

make || return 1

make install || return 1

# Xvid uses xvidcore.a for MinGW builds, but -lxvidcore expects the
# conventional libxvidcore.a archive name.
mv "${LIB_INSTALL_PREFIX}"/lib/xvidcore.a \
  "${LIB_INSTALL_PREFIX}"/lib/libxvidcore.a || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_xvidcore_package_config "1.3.7" || return 1

# WORKAROUND TO REMOVE DYNAMIC LIBS
rm -f "${LIB_INSTALL_PREFIX}"/lib/*xvidcore.dll* 1>>"${BASEDIR}"/build.log 2>&1

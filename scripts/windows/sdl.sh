#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_sdl} -eq 1 ]]; then
  ./autogen.sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

# SDL's configure REFUSES TO RUN IN-TREE ON A git CLONE, BECAUSE GENERATING
# SDL_config.h AND SDL_revision.h WOULD CLOBBER THE CHECKED-IN COPIES. IT
# DETECTS THIS AS `srcdir == "."` PLUS A .git DIRECTORY, SO THE ONLY FIX IS TO
# CONFIGURE FROM A SEPARATE DIRECTORY. ${BUILD_DIR} IS ALREADY EMPTIED BY
# run-windows.sh BEFORE THIS SCRIPT RUNS, SO NO distclean IS NEEDED.
mkdir -p "${BUILD_DIR}" || return 1
cd "${BUILD_DIR}" || return 1

"${BASEDIR}"/src/"${LIB_NAME}"/configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --disable-video-x11 \
  --disable-video-wayland \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-hidapi \
  --disable-sensor \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

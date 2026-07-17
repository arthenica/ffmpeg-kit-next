#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libvorbis} -eq 1 ]]; then

  # -mno-ieee-fp OPTION IS NOT COMPATIBLE WITH clang. REMOVING IT
  ${SED_INLINE} 's/\-mno-ieee-fp//g' "${BASEDIR}"/src/"${LIB_NAME}"/configure.ac 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

PKG_CONFIG= emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-ogg-includes="${LIB_INSTALL_BASE}"/libogg/include \
  --with-ogg-libraries="${LIB_INSTALL_BASE}"/libogg/lib \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-docs \
  --disable-examples \
  --disable-oggtest \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp vorbis.pc vorbisenc.pc vorbisfile.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

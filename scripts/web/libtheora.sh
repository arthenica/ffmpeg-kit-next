#!/bin/bash

# SET BUILD OPTIONS
export OGG_CFLAGS="$(pkg-config --cflags ogg 2>>"${BASEDIR}"/build.log)"
export OGG_LIBS="$(pkg-config --libs-only-L ogg 2>>"${BASEDIR}"/build.log)"
export VORBIS_CFLAGS="$(pkg-config --cflags vorbis 2>>"${BASEDIR}"/build.log)"
export VORBIS_LIBS="$(pkg-config --libs-only-L vorbis 2>>"${BASEDIR}"/build.log)"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libtheora} -eq 1 ]]; then

  # WORKAROUND NOT TO RUN CONFIGURE AT THE END OF autogen.sh
  ${SED_INLINE} 's/$srcdir\/configure/#$srcdir\/configure/g' "${BASEDIR}"/src/"${LIB_NAME}"/autogen.sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  ./autogen.sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-examples \
  --disable-telemetry \
  --disable-sdltest \
  --disable-asm \
  --disable-valgrind-testing \
  --disable-spec \
  --disable-oggtest \
  --disable-vorbistest \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# DELETE ENCODER/DECODER SPECIFIC ARCHIVES SINCE WE DONT NEED THEM
# THEY BOTH INCLUDE COMMON SYMBOLS AND CAUSE DUPLICATE SYMBOL ERRORS IN FFMPEG
rm -f "${LIB_INSTALL_BASE}/${LIB_NAME}"/lib/libtheoraenc.*
rm -f "${LIB_INSTALL_BASE}/${LIB_NAME}"/lib/libtheoradec.*
rm -f "${LIB_INSTALL_BASE}/${LIB_NAME}"/lib/pkgconfig/theoradec.pc
rm -f "${LIB_INSTALL_BASE}/${LIB_NAME}"/lib/pkgconfig/theoraenc.pc

# MANUALLY COPY PKG-CONFIG FILES
cp theora.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

#!/bin/bash

# UPDATE BUILD FLAGS
export CFLAGS="${CFLAGS} -I${LIB_INSTALL_BASE}/giflib/include"
export CXXFLAGS="${CXXFLAGS}"
export CPPFLAGS="-I${LIB_INSTALL_BASE}/giflib/include"
export LDFLAGS="${LDFLAGS} -L${LIB_INSTALL_BASE}/giflib/lib"

export LIBPNG_CFLAGS="$(pkg-config --cflags libpng 2>>"${BASEDIR}"/build.log)"
export LIBPNG_LIBS="$(pkg-config --libs-only-L libpng 2>>"${BASEDIR}"/build.log)"

export LIBWEBP_CFLAGS="$(pkg-config --cflags libwebp 2>>"${BASEDIR}"/build.log)"
export LIBWEBP_LIBS="$(pkg-config --libs-only-L libwebp 2>>"${BASEDIR}"/build.log)"

export LIBTIFF_CFLAGS="$(pkg-config --cflags libtiff-4 2>>"${BASEDIR}"/build.log)"
export LIBTIFF_LIBS="$(pkg-config --libs-only-L libtiff-4 2>>"${BASEDIR}"/build.log)"

export ZLIB_CFLAGS="$(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)"
export ZLIB_LIBS="$(pkg-config --libs-only-L zlib 2>>"${BASEDIR}"/build.log)"

export JPEG_CFLAGS="$(pkg-config --cflags libjpeg 2>>"${BASEDIR}"/build.log)"
export JPEG_LIBS="$(pkg-config --libs-only-L libjpeg 2>>"${BASEDIR}"/build.log)"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_leptonica} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-zlib \
  --with-libpng \
  --with-jpeg \
  --with-giflib \
  --with-libtiff \
  --with-libwebp \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-programs \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp lept.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

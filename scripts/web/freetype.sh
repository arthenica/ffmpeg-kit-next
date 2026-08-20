#!/bin/bash

# UPDATE BUILD FLAGS
export LIBPNG_CFLAGS="-I${LIB_INSTALL_BASE}/libpng/include"
export LIBPNG_LIBS="-L${LIB_INSTALL_BASE}/libpng/lib"
export BROTLI_CFLAGS="$(pkg-config --cflags libbrotlidec 2>>"${BASEDIR}"/build.log)" || return 1
export BROTLI_LIBS="$(pkg-config --libs --static libbrotlidec 2>>"${BASEDIR}"/build.log)" || return 1
export ZLIB_CFLAGS="$(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)" || return 1
export ZLIB_LIBS="$(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/builds/unix/configure ]] || [[ ${RECONF_freetype} -eq 1 ]]; then

  # NOTE THAT FREETYPE DOES NOT SUPPORT AUTORECONF BUT IT COMES WITH AN autogen.sh
  ./autogen.sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-zlib \
  --with-png \
  --with-brotli \
  --without-harfbuzz \
  --without-bzip2 \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-mmap \
  --build="$("${BASEDIR}"/src/"${LIB_NAME}"/builds/unix/config.guess)" \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_freetype_package_config "26.6.20" || return 1

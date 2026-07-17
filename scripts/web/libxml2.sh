#!/bin/bash

# UPDATE BUILD FLAGS
export Z_CFLAGS="$(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)" || return 1
export Z_LIBS="$(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libxml2} -eq 1 ]]; then
  ${SED_INLINE} 's|^AC_PREREQ|#AC_PREREQ|g' "${BASEDIR}"/src/"${LIB_NAME}"/configure.ac 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} 's|AM_INIT_AUTOMAKE(\[[0-9.]* |AM_INIT_AUTOMAKE(\[|g' "${BASEDIR}"/src/"${LIB_NAME}"/configure.ac 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-zlib \
  --with-iconv \
  --with-sax1 \
  --without-http \
  --without-ftp \
  --without-python \
  --without-debug \
  --without-lzma \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) AM_LDFLAGS="${Z_LIBS}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install AM_LDFLAGS="${Z_LIBS}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_libxml2_package_config "2.11.4" || return 1

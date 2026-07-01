#!/bin/bash

export ac_cv_va_copy=C99
export FREETYPE_CFLAGS="$(pkg-config --cflags freetype2)"
export FREETYPE_LIBS="$(pkg-config --libs --static freetype2)"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# WORKAROUND FOR "bad flag in substitute command"
${SED_INLINE} "s|in \"\$default_fonts\"|in \$default_fonts|g" "${BASEDIR}"/src/"${LIB_NAME}"/configure.ac 1>>"${BASEDIR}"/build.log 2>&1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_fontconfig} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

${SED_INLINE} "s/NO_HAVE_POSIX_FADVISE/HAVE_POSIX_FADVISE/g" "${BASEDIR}"/src/"${LIB_NAME}"/src/fccache.c 1>>"${BASEDIR}"/build.log 2>&1 || return 1

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-libiconv-prefix="${LIB_INSTALL_BASE}"/libiconv \
  --with-expat="${LIB_INSTALL_BASE}"/expat \
  --without-libintl-prefix \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-cache-build \
  --disable-rpath \
  --disable-libxml2 \
  --disable-docs \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_fontconfig_package_config "2.18.1" || return 1

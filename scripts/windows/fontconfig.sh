#!/bin/bash

export ac_cv_va_copy=C99
export FREETYPE_CFLAGS="$(pkg-config --cflags freetype2)"
export FREETYPE_LIBS="$(pkg-config --libs --static freetype2)"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# WORKAROUND FOR "bad flag in substitute command"
${SED_INLINE} "s|in \"\$default_fonts\"|in \$default_fonts|g" "${BASEDIR}"/src/"${LIB_NAME}"/configure.ac 1>>"${BASEDIR}"/build.log 2>&1

# fontconfig uses AM_GNU_GETTEXT, so autoreconf would invoke MSYS2's broken
# autopoint. Seed the gettext aux files (config.rpath and po/, po-conf/
# Makefile.in.in) it would otherwise install and skip autopoint. These are
# inputs to both autoreconf AND configure/config.status, so seed them on every
# run - not only when regenerating - otherwise a re-run with an existing
# configure still fails in config.status. fontconfig has no AC_CONFIG_AUX_DIR,
# so its aux directory is the source root (".").
seed_gettext_autotools_aux "." || return 1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_fontconfig} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-libiconv-prefix="${LIB_INSTALL_BASE}"/libiconv \
  --with-expat="${LIB_INSTALL_BASE}"/expat \
  --without-libintl-prefix \
  --disable-nls \
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

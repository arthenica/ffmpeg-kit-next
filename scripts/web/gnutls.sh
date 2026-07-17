#!/bin/bash

# INIT SUBMODULES
${SED_INLINE} 's|openssl/openssl|arthenica/openssl|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|tomato42|arthenica|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|warner|arthenica|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/libidn/gnulib-mirror|github.com/arthenica/gnulib|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/gnutls/libtasn1|github.com/arthenica/libtasn1|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/gnutls/nettle|github.com/arthenica/nettle|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/gnutls/abi-dump|github.com/arthenica/abi-dump|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/gnutls/cligen|github.com/arthenica/cligen|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1
${SED_INLINE} 's|gitlab.com/redhat-crypto/tests/interop|github.com/arthenica/redhat-crypto-tests-interop|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# UPDATE BUILD FLAGS
export CFLAGS="$(get_cflags ${LIB_NAME})"
export CXXFLAGS="$(get_cxxflags "${LIB_NAME}")"
export LDFLAGS="$(get_ldflags ${LIB_NAME})"

export NETTLE_CFLAGS="-I${LIB_INSTALL_BASE}/nettle/include"
export NETTLE_LIBS="-L${LIB_INSTALL_BASE}/nettle/lib -lnettle -L${LIB_INSTALL_BASE}/gmp/lib -lgmp"
export HOGWEED_CFLAGS="-I${LIB_INSTALL_BASE}/nettle/include"
export HOGWEED_LIBS="-L${LIB_INSTALL_BASE}/nettle/lib -lhogweed -L${LIB_INSTALL_BASE}/gmp/lib -lgmp"
export GMP_CFLAGS="-I${LIB_INSTALL_BASE}/gmp/include"
export GMP_LIBS="-L${LIB_INSTALL_BASE}/gmp/lib -lgmp"

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_gnutls} -eq 1 ]]; then
  git submodule update --init gnulib 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  # Newer autopoint treats AM_GNU_GETTEXT_VERSION and the guarded
  # AM_GNU_GETTEXT_REQUIRE_VERSION compatibility block as duplicate version
  # declarations. Keep the canonical AM_GNU_GETTEXT_VERSION line.
  ${SED_INLINE} '/^m4_ifdef(\[AM_GNU_GETTEXT_REQUIRE_VERSION\],\[$/,/^])$/d' ./configure.ac 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  # Docs are disabled for wasm, but GnuTLS bootstrap still probes gtk-doc.
  # Provide the minimal macro/makefile pieces needed by autoreconf.
  mkdir -p ./m4 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  cat >./m4/gtk-doc.m4 <<'GTK_DOC_M4_EOF' || return 1
AC_DEFUN([GTK_DOC_CHECK],
[
  AC_ARG_ENABLE([gtk-doc], [], [enable_gtk_doc=$enableval], [enable_gtk_doc=no])
  AC_ARG_ENABLE([gtk-doc-html], [], [enable_gtk_doc_html=$enableval], [enable_gtk_doc_html=no])
  AC_ARG_ENABLE([gtk-doc-pdf], [], [enable_gtk_doc_pdf=$enableval], [enable_gtk_doc_pdf=no])
  HTML_DIR='${datadir}/gtk-doc/html'
  AC_SUBST([HTML_DIR])
  AM_CONDITIONAL([HAVE_GTK_DOC], [false])
  AM_CONDITIONAL([ENABLE_GTK_DOC], [test "x$enable_gtk_doc" = "xyes"])
  AM_CONDITIONAL([GTK_DOC_BUILD_HTML], [test "x$enable_gtk_doc_html" = "xyes"])
  AM_CONDITIONAL([GTK_DOC_BUILD_PDF], [test "x$enable_gtk_doc_pdf" = "xyes"])
  AM_CONDITIONAL([GTK_DOC_USE_LIBTOOL], [false])
  AM_CONDITIONAL([GTK_DOC_USE_REBASE], [false])
])
GTK_DOC_M4_EOF

  GNUTLS_BOOTSTRAP_BIN="${FFMPEG_KIT_TMPDIR}/gnutls-bootstrap-bin"
  mkdir -p "${GNUTLS_BOOTSTRAP_BIN}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  cat >"${GNUTLS_BOOTSTRAP_BIN}/gtkdocize" <<'GTKDOCIZE_EOF' || return 1
#!/bin/sh
if [ "$1" = "--version" ]; then
  echo "gtkdocize (ffmpeg-kit web stub)"
  exit 0
fi
cat > gtk-doc.make <<'GTK_DOC_MAKE_EOF'
EXTRA_DIST =
CLEANFILES =
GTK_DOC_MAKE_EOF
exit 0
GTKDOCIZE_EOF
  chmod +x "${GNUTLS_BOOTSTRAP_BIN}/gtkdocize" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  PATH="${GNUTLS_BOOTSTRAP_BIN}:${PATH}" ./bootstrap --skip-po --no-git --gnulib-srcdir=gnulib 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  overwrite_file ./gnulib/lib/fpending.c ./src/gl/fpending.c 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

if [[ -f "${BASEDIR}"/src/"${LIB_NAME}"/build-aux/config.guess ]]; then
  overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.guess "${BASEDIR}"/src/"${LIB_NAME}"/build-aux/config.guess 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi
if [[ -f "${BASEDIR}"/src/"${LIB_NAME}"/build-aux/config.sub ]]; then
  overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.sub "${BASEDIR}"/src/"${LIB_NAME}"/build-aux/config.sub 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

export gl_cv_func_nanosleep=yes
export gl_cv_func_sleep_works=yes

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-included-libtasn1 \
  --with-included-unistring \
  --without-idn \
  --without-p11-kit \
  --without-brotli \
  --without-zlib \
  --disable-hardware-acceleration \
  --enable-static \
  --disable-openssl-compatibility \
  --disable-shared \
  --disable-fast-install \
  --disable-code-coverage \
  --disable-doc \
  --disable-gtk-doc \
  --disable-gtk-doc-html \
  --disable-gtk-doc-pdf \
  --disable-manpages \
  --disable-guile \
  --disable-tests \
  --disable-tools \
  --disable-maintainer-mode \
  --disable-full-test-suite \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_gnutls_package_config "3.7.11" || return 1

#!/bin/bash

# INIT SUBMODULES
${SED_INLINE} 's|openssl/openssl|arthenica/openssl|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|tomato42|arthenica|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|warner|arthenica|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/libidn/gnulib-mirror|github.com/arthenica/gnulib|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/gnutls/libtasn1|github.com/arthenica/libtasn1|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/gnutls/nettle|github.com/arthenica/nettle|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/gnutls/abi-dump|github.com/arthenica/abi-dump|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/gnutls/cligen|github.com/arthenica/cligen|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1
${SED_INLINE} 's|gitlab.com/redhat-crypto/tests/interop|github.com/arthenica/redhat-crypto-tests-interop|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1

# UPDATE BUILD FLAGS
export CFLAGS="$(get_cflags ${LIB_NAME}) -I${LIB_INSTALL_BASE}/libiconv/include"
export CXXFLAGS=$(get_cxxflags "${LIB_NAME}")
export LDFLAGS="$(get_ldflags ${LIB_NAME}) -L${LIB_INSTALL_BASE}/libiconv/lib"

export NETTLE_CFLAGS="-I${LIB_INSTALL_BASE}/nettle/include"
export NETTLE_LIBS="-L${LIB_INSTALL_BASE}/nettle/lib -lnettle -L${LIB_INSTALL_BASE}/gmp/lib -lgmp"
export HOGWEED_CFLAGS="-I${LIB_INSTALL_BASE}/nettle/include"
export HOGWEED_LIBS="-L${LIB_INSTALL_BASE}/nettle/lib -lhogweed -L${LIB_INSTALL_BASE}/gmp/lib -lgmp"
export GMP_CFLAGS="-I${LIB_INSTALL_BASE}/gmp/include"
export GMP_LIBS="-L${LIB_INSTALL_BASE}/gmp/lib -lgmp"
export ZLIB_CFLAGS="$(pkg-config --cflags zlib 2>>"${BASEDIR}"/build.log)" || return 1
export ZLIB_LIBS="$(pkg-config --libs --static zlib 2>>"${BASEDIR}"/build.log)" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_gnutls} -eq 1 ]]; then
  git submodule update --init gnulib || return 1

  # GnuTLS 3.7.11 pins a gnulib revision that passes a Windows HANDLE to
  # FD_ISSET, which expects a SOCKET. Check out the upstream gnulib fix before
  # bootstrap imports lib/select.c into src/gl/select.c.
  GNULIB_WINDOWS_SELECT_FIX="21fccfa0451ba59fba479e439465da9c360353d3"
  git -C gnulib fetch origin "${GNULIB_WINDOWS_SELECT_FIX}" || return 1
  git -C gnulib checkout --detach "${GNULIB_WINDOWS_SELECT_FIX}" || return 1

  # MSYS2's autopoint (from gettext-devel) ships WITHOUT the gettext
  # infrastructure archive (/usr/share/gettext/archive.dir.tar.xz), so the
  # "autopoint --force" step inside ./bootstrap fails with:
  #   infrastructure files for version ... not found
  # gnulib already carries the gettext m4 macros, config.rpath and the po
  # Makefile template, and bootstrap runs gnulib-tool right after autopoint and
  # overwrites everything autopoint installs with newer copies. So we seed the
  # files autopoint would provide from the bundled gnulib tree and skip
  # autopoint (AUTOPOINT=true). --disable-nls (below) keeps the po/ directory
  # from being built, so the translation-only helper files that gnulib does not
  # ship are never needed.
  mkdir -p m4 po build-aux 1>>"${BASEDIR}"/build.log 2>&1
  for gettext_m4 in gettext iconv lib-ld lib-link lib-prefix nls po progtest; do
    overwrite_file "gnulib/m4/${gettext_m4}.m4" "m4/${gettext_m4}.m4" || return 1
  done
  overwrite_file "gnulib/build-aux/config.rpath" "build-aux/config.rpath" || return 1
  overwrite_file "gnulib/build-aux/po/Makefile.in.in" "po/Makefile.in.in" || return 1
  overwrite_file "gnulib/build-aux/po/remove-potcdate.sin" "po/remove-potcdate.sin" || return 1

  # bootstrap generates po/Makevars from po/Makevars.template, another file that
  # autopoint normally installs and gnulib does not ship. Provide the standard
  # gettext template so the generation step succeeds (its content is inert with
  # --disable-nls; bootstrap only needs the file to be readable).
  seed_gettext_po_makevars "po" || return 1

  AUTOPOINT=true ./bootstrap --skip-po --no-git --gnulib-srcdir=gnulib || return 1
fi

# WINDOWS FIX: gnutls_system_recv_timeout() calls winsock select() with a
# 'struct timeval', but on MinGW gnulib replaces 'struct timeval' with a wider
# rpl_timeval (64-bit tv_sec) whose layout does NOT match winsock's native
# TIMEVAL (32-bit tv_sec). Passing the gnulib struct silently drops tv_usec, so
# sub-second receive timeouts collapse to 0, and recent clang rejects the type
# mismatch as a hard -Wincompatible-pointer-types error. TIMEVAL is winsock's
# typedef, fixed to the native struct at winsock2.h parse time and unaffected by
# gnulib's macro, so use it for the select() timeout. (lib/system.c keeps the
# gnulib timeval; there it feeds gnulib's gettimeofday, which expects it.)
if grep -q "struct timeval _tv, \*tv = NULL;" lib/system/sockets.c; then
  ${SED_INLINE} 's|struct timeval _tv, \*tv = NULL;|TIMEVAL _tv, *tv = NULL;|' lib/system/sockets.c || return 1
fi

# WINDOWS FIX: define top_builddir in po/Makefile.in.in.
#
# gettext's po/Makefile.in.in never defines top_builddir itself; it normally
# arrives as the "subdir"/"top_builddir" pair inside po/Makevars, which the
# po-directories config.status command splices in at the "# Makevars" marker.
# Because we skip autopoint, that Makevars is unreliable (empty here), leaving
# $(top_builddir) undefined so "make" in po/ resolves "$(top_builddir)/config.
# status" to "/config.status" and dies with "No rule to make target". Define it
# directly via the standard @top_builddir@ substitution (config.status expands
# it to ".." for po/), which does not depend on Makevars. Runs on every build
# (not only when bootstrapping) so it applies even when configure is reused; the
# grep guard keeps it idempotent.
if [ -f po/Makefile.in.in ] && ! grep -q '^top_builddir =' po/Makefile.in.in; then
  ${SED_INLINE} '/^top_srcdir = @top_srcdir@/a top_builddir = @top_builddir@' po/Makefile.in.in || return 1
fi

# SET HARDWARE ACCELERATION OPTIONS
#
# gnutls ships hand-written accelerated crypto assembly per object format. Its
# x86/x86-64 sources have a COFF (Windows) variant, but its aarch64 sources only
# have elf/ and macosx/ variants (see lib/accelerated/aarch64/Makefile.am). On
# Windows-on-ARM (PE/COFF) it falls back to the elf/ variant, whose ELF-only
# directives (.hidden, .type ...,%function) the assembler rejects. There is no
# COFF aarch64 asm to use, so hardware acceleration is disabled for arm64 and
# gnutls falls back to the portable C/nettle implementations.
case ${ARCH} in
arm64)
  ASM_OPTIONS="--disable-hardware-acceleration"
  ;;
*)
  ASM_OPTIONS="--enable-hardware-acceleration"
  ;;
esac

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --with-included-libtasn1 \
  --with-included-unistring \
  --without-idn \
  --without-p11-kit \
  --without-brotli \
  ${ASM_OPTIONS} \
  --enable-static \
  --disable-nls \
  --disable-openssl-compatibility \
  --disable-shared \
  --disable-fast-install \
  --disable-code-coverage \
  --disable-doc \
  --disable-manpages \
  --disable-guile \
  --disable-tests \
  --disable-tools \
  --disable-maintainer-mode \
  --disable-full-test-suite \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_gnutls_package_config "3.7.11" || return 1

#!/bin/bash

# FIX HARD-CODED PATHS
${SED_INLINE} 's|git://git.savannah.gnu.org|https://github.com/arthenica|g' "${BASEDIR}"/src/"${LIB_NAME}"/.gitmodules || return 1

if [[ ! -d "${BASEDIR}"/src/"${LIB_NAME}"/gnulib ]]; then

  # INIT SUBMODULES
  ./gitsub.sh pull || return 1
  ./gitsub.sh checkout gnulib 485d983b7795548fb32b12fbe8370d40789e88c4 || return 1
fi

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_libiconv} -eq 1 ]]; then

  # lib/aliases.h AND FRIENDS ARE GENERATED FILES THAT ARE NOT IN VERSION
  # CONTROL, SO autogen.sh REGENERATES THEM WITH gperf.
  if ! [ -x "$(command -v gperf)" ]; then
    echo -e "\n(*) gperf command not found. Install it with 'pacman -S gperf'\n"
    return 1
  fi

  # UNDO THE WORKAROUNDS BELOW SO THEY STAY IDEMPOTENT ACROSS RECONFIGURES
  git checkout "${BASEDIR}"/src/"${LIB_NAME}"/Makefile.devel 1>>"${BASEDIR}"/build.log 2>&1
  git checkout "${BASEDIR}"/src/"${LIB_NAME}"/libcharset/Makefile.devel 1>>"${BASEDIR}"/build.log 2>&1

  # autogen.sh DRIVES Makefile.devel, WHICH HARD-CODES ITS MAINTAINER TOOLS.
  # THE THREE WORKAROUNDS BELOW MAKE THOSE TOOLS MATCH AN MSYS2 INSTALL.

  # 1. Makefile.devel COMPILES THE genaliases HELPER WITH A HARD-CODED gcc. THE
  # MinGW-w64 ENVIRONMENTS THAT TARGET arm64 SHIP clang AND NO gcc AT ALL, SO
  # USE THE COMPILER RESOLVED BY set_toolchain_paths INSTEAD. genaliases IS RUN
  # IMMEDIATELY AFTER IT IS BUILT, WHICH IS SAFE HERE BECAUSE WINDOWS BUILDS ARE
  # NATIVE AND THE TARGET COMPILER PRODUCES BINARIES THE HOST CAN EXECUTE.
  ${SED_INLINE} "s|^CC = gcc -Wall|CC = ${CC} -Wall|g" "${BASEDIR}"/src/"${LIB_NAME}"/Makefile.devel || return 1

  # 2. THE automake/aclocal NAMES ARE PINNED TO 1.16. MSYS2 SHIPS NEWER
  # VERSIONS UNDER THE UNVERSIONED NAMES ONLY.
  ${SED_INLINE} 's|^AUTOMAKE = automake-1.16|AUTOMAKE = automake|g' "${BASEDIR}"/src/"${LIB_NAME}"/Makefile.devel || return 1
  ${SED_INLINE} 's|^ACLOCAL = aclocal-1.16|ACLOCAL = aclocal|g' "${BASEDIR}"/src/"${LIB_NAME}"/Makefile.devel || return 1
  ${SED_INLINE} 's|^ACLOCAL = aclocal-1.16|ACLOCAL = aclocal|g' "${BASEDIR}"/src/"${LIB_NAME}"/libcharset/Makefile.devel || return 1

  # 3. THE all TARGET ALSO RENDERS THE MAN PAGES TO HTML WITH groff, WHICH IS
  # NOT PART OF A MINIMAL MSYS2 INSTALL. THE HTML DOCS ARE OF NO USE TO
  # FFmpegKit, SO DROP THEM FROM THE PREREQUISITES. ONLY THE CONTINUATION LINE
  # INSIDE all IS INDENTED WITH SPACES; THE clean RULE THAT ALSO LISTS THESE
  # FILES IS TAB-INDENTED AND STAYS UNTOUCHED.
  ${SED_INLINE} '/^ *man\/iconv\.1\.html/d' "${BASEDIR}"/src/"${LIB_NAME}"/Makefile.devel || return 1

  ./autogen.sh || return 1
fi

# man/Makefile.in INSTALLS *.html WITH AN UNGUARDED GLOB, SO WITH THE HTML DOCS
# NO LONGER GENERATED ITS install TARGET FAILS ON THE UNEXPANDED PATTERN. DROP
# THE HTML INSTALL/UNINSTALL LINES TO MATCH. THE VARIABLE DEFINITION
# "htmldir = @htmldir@" HAS NO ")" AND IS DELIBERATELY LEFT ALONE.
#
# THIS RUNS OUTSIDE THE REGENERATION BLOCK ABOVE, BECAUSE man/Makefile IS
# PRODUCED BY ./configure ON EVERY BUILD, NOT ONLY WHEN autogen.sh RUNS.
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/man/Makefile.in 1>>"${BASEDIR}"/build.log 2>&1
${SED_INLINE} '/\*\.html/d' "${BASEDIR}"/src/"${LIB_NAME}"/man/Makefile.in || return 1
${SED_INLINE} '/htmldir)/d' "${BASEDIR}"/src/"${LIB_NAME}"/man/Makefile.in || return 1

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-rpath \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_libiconv_package_config "1.17" || return 1

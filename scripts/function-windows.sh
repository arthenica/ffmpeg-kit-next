#!/bin/bash

source "${BASEDIR}/scripts/function.sh"

prepare_inline_sed || exit 1

# pkgconf ON MSYS2 ENABLES --define-prefix BY DEFAULT: FOR EACH PACKAGE IT
# RECOMPUTES `prefix` FROM THE .pc FILE'S LOCATION AND REWRITES includedir/libdir
# ACCORDINGLY. EVERY FFMPEGKIT LIBRARY INSTALLS INTO ITS OWN PREFIX
# (prebuilt/<build>/<lib>) BUT COPIES ITS .pc INTO ONE SHARED pkgconfig DIRECTORY,
# SO define-prefix COLLAPSES EVERY -I/-L DOWN TO prebuilt/{include,lib} (WHICH DO
# NOT EXIST) AND BUILDS FAIL WITH "'<header>' file not found". OUR .pc FILES ALREADY
# CARRY ABSOLUTE PATHS AND NEVER NEED RELOCATION, SO DISABLE define-prefix FOR THE
# WHOLE WINDOWS BUILD. HONORED BY BOTH pkg-config AND THE ${HOST}-pkg-config WRAPPER.
export PKG_CONFIG_DONT_DEFINE_PREFIX=1

# MSYS2 SHIPS ninja (meson REQUIRES IT) AND cmake PREFERS THE Ninja GENERATOR
# WHENEVER IT IS ON PATH. THE cmake BASED LIBRARY SCRIPTS BUILD WITH make, SO
# PIN THE GENERATOR FOR THE WHOLE WINDOWS BUILD RATHER THAN LETTING IT DEPEND ON
# WHICH TOOLS HAPPEN TO BE INSTALLED. cmake READS THIS VARIABLE AS ITS DEFAULT
# GENERATOR, AND EVERY SCRIPT DELETES ITS BUILD DIRECTORY BEFORE CONFIGURING, SO
# THERE IS NO CACHED GENERATOR TO CONFLICT WITH.
export CMAKE_GENERATOR="Unix Makefiles"

# WINDOWS LIBRARIES ARE BUILT WITH THE MinGW-w64 TOOLCHAIN UNDER MSYS2 AND ARE
# COMPILED NATIVELY, SO ONLY THE ARCHITECTURE OF THE HOST MACHINE CAN BE BUILT.
# arm64 (aarch64) IS THE PRIMARY TARGET AND REQUIRES AN arm64 WINDOWS HOST
# (MSYS2 CLANGARM64); x86-64 REQUIRES AN x86-64 HOST.
enable_default_windows_architectures() {
  # Under MSYS2, `uname -m` reports the architecture of the MSYS2 runtime
  # process, which on a Windows-on-ARM host is an emulated x86_64 process even
  # inside the arm64-native CLANGARM64 environment. It does NOT describe the
  # architecture the active MinGW toolchain targets. Prefer the MSYS2 variables
  # that describe the MinGW target (MSYSTEM_CARCH / MINGW_CHOST) and only fall
  # back to `uname -m` outside MSYS2.
  local host_arch="${MSYSTEM_CARCH:-}"

  # Derive from the MinGW target triplet when MSYSTEM_CARCH is not exported.
  if [[ -z ${host_arch} && -n ${MINGW_CHOST} ]]; then
    host_arch="${MINGW_CHOST%%-*}"
  fi

  # Fall back to the MSYSTEM name, which is the most reliably-set signal.
  if [[ -z ${host_arch} && -n ${MSYSTEM} ]]; then
    case "${MSYSTEM}" in
    CLANGARM64)
      host_arch="aarch64"
      ;;
    MINGW64 | UCRT64 | CLANG64)
      host_arch="x86_64"
      ;;
    esac
  fi

  # Outside MSYS2, `uname -m` is accurate.
  if [[ -z ${host_arch} ]]; then
    host_arch=$(uname -m)
  fi

  case "${host_arch}" in
  aarch64 | arm64)
    ENABLED_ARCHITECTURES[ARCH_ARM64]=1
    ;;
  *)
    ENABLED_ARCHITECTURES[ARCH_X86_64]=1
    ;;
  esac
}

set_default_min_windows_platform_version() {
  local _TMP
}

get_ffmpeg_kit_version() {
  local FFMPEG_KIT_VERSION=$(sed -n 's/.*FFmpegKitVersion = "\([^"]*\)".*/\1/p' "${BASEDIR}/windows/src/FFmpegKitConfig.h" 2>>"${BASEDIR}"/build.log)

  echo "${FFMPEG_KIT_VERSION}"
}

get_target_cpu() {
  case ${ARCH} in
  arm64)
    echo "aarch64"
    ;;
  x86-64)
    echo "x86_64"
    ;;
  esac
}

get_host() {
  echo "$(get_target_cpu)-w64-mingw32"
}

get_cmake_system_processor() {
  case ${ARCH} in
  arm64)
    echo "ARM64"
    ;;
  x86-64)
    echo "AMD64"
    ;;
  esac
}

get_build_directory() {
  echo "windows-$(get_target_cpu)"
}

get_bundle_directory() {
  echo "bundle-windows"
}

get_pkg_config_libdir() {
  local PKG_CONFIG_LIBDIR_VALUE="${INSTALL_PKG_CONFIG_DIR}"

  if [[ -n ${FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR} ]]; then
    PKG_CONFIG_LIBDIR_VALUE+=":${FFMPEG_KIT_SYSTEM_PKG_CONFIG_LIBDIR}"
  fi

  echo "${PKG_CONFIG_LIBDIR_VALUE}"
}

get_common_cflags() {
  echo "-DWINDOWS"
}

get_arch_specific_cflags() {
  case ${ARCH} in
  arm64)
    echo "-march=armv8-a -DFFMPEG_KIT_ARM64"
    ;;
  x86-64)
    echo "-march=x86-64 -msse4.2 -mpopcnt -m64 -DFFMPEG_KIT_X86_64"
    ;;
  esac
}

get_size_optimization_cflags() {
  echo "-Os -ffunction-sections -fdata-sections"
}

get_app_specific_cflags() {
  local APP_FLAGS=""
  case $1 in
  ffmpeg)
    APP_FLAGS="-Wno-unused-function"
    ;;
  ffmpeg-kit)
    APP_FLAGS="-Wno-unused-function -Wno-pointer-sign -Wno-switch -Wno-deprecated-declarations $(get_package_name_cflag)"
    ;;
  fontconfig)
    # WINDOWS FIX: fcstr.c picks its FcStrDupVapFormat implementation from the
    # printf function configure detected. On MinGW configure finds HAVE_VASPRINTF,
    # whose code path calls FcLocaleSetCurrent() - a POSIX uselocale() wrapper that
    # fontconfig declares and defines only "#ifndef _WIN32" (Windows has no
    # uselocale). The call is therefore undeclared, which recent clang rejects as a
    # hard -Wimplicit-function-declaration error. The implementation fontconfig
    # intends for Windows uses _vsnprintf_l(), which takes the locale directly and
    # needs no FcLocaleSetCurrent. MinGW-w64 provides _vsnprintf_l (via
    # <sec_api/stdio_s.h>) and _create_locale/_free_locale, but configure never
    # probes for it, so select that code path explicitly. HAVE__VSNPRINTF_L is not
    # emitted into config.h, so defining it on the command line is safe.
    APP_FLAGS="-std=c99 -Wno-unused-function -DHAVE__VSNPRINTF_L=1"
    ;;
  gnutls)
    APP_FLAGS="-std=c99 -Wno-unused-function -D_GL_USE_STDLIB_ALLOC=1"
    ;;
  kvazaar | libsvtav1)
    APP_FLAGS="-std=gnu99 -Wno-unused-function"
    ;;
  libaom)
    APP_FLAGS="-std=gnu99 -Wno-unused-function -Wno-implicit-function-declaration"
    ;;
  libvpx | openssl | shine | srt)
    APP_FLAGS="-Wno-unused-function"
    ;;
  openh264)
    APP_FLAGS="-std=gnu99 -Wno-unused-function -fstack-protector-all"
    ;;
  rubberband)
    APP_FLAGS="-std=c99 -Wno-unused-function"
    ;;
  sdl)
    APP_FLAGS="-std=c99 -Wno-unused-function -Wno-incompatible-function-pointer-types"
    ;;
  soxr | snappy | libwebp)
    APP_FLAGS="-std=gnu99 -Wno-unused-function -DPIC"
    ;;
  xvidcore)
    APP_FLAGS="-std=c99 -DWIN32"
    ;;
  *)
    APP_FLAGS="-std=c99 -Wno-unused-function"
    ;;
  esac

  echo "${APP_FLAGS}"
}

get_cflags() {
  local ARCH_FLAGS=$(get_arch_specific_cflags)
  local APP_FLAGS=$(get_app_specific_cflags "$1")
  local COMMON_FLAGS=$(get_common_cflags)
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS=$(get_size_optimization_cflags "$1")
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi

  echo "${ARCH_FLAGS} ${APP_FLAGS} ${COMMON_FLAGS} ${OPTIMIZATION_FLAGS} ${EXTRA_CFLAGS}"
}

get_cxxflags() {
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS="-Os -ffunction-sections -fdata-sections"
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi

  local BUILD_DATE="-DFFMPEG_KIT_BUILD_DATE=$(date +%Y%m%d 2>>"${BASEDIR}"/build.log)"
  if [[ -z ${NO_FFMPEG_KIT_PROTOCOLS} ]]; then
    local USES_FFMPEG_KIT_PROTOCOLS="-DUSES_FFMPEG_KIT_PROTOCOLS"
  else
    local USES_FFMPEG_KIT_PROTOCOLS=""
  fi
  local COMMON_FLAGS="${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE} $(get_common_cflags) $(get_arch_specific_cflags)"

  case $1 in
  ffmpeg)
    if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
      echo "-std=c++11 -O2 -ffunction-sections -fdata-sections ${EXTRA_CXXFLAGS}"
    else
      echo "${FFMPEG_KIT_DEBUG} -std=c++11 ${EXTRA_CXXFLAGS}"
    fi
    ;;
  ffmpeg-kit)
    echo "-std=c++11 ${COMMON_FLAGS} $(get_package_name_cflag) ${USES_FFMPEG_KIT_PROTOCOLS}"
    ;;
  opencore-amr)
    echo "${COMMON_FLAGS}"
    ;;
  gnutls)
    echo "-std=c++11 ${COMMON_FLAGS} -fno-rtti"
    ;;
  libjxl)
    echo "-std=c++17 ${COMMON_FLAGS}"
    ;;
  vvenc)
    echo "-std=c++14 ${COMMON_FLAGS}"
    ;;
  x265)
    echo "-std=c++11 ${COMMON_FLAGS} -fno-exceptions"
    ;;
  libsvtav1 | rubberband | srt | tesseract | zimg)
    echo "-std=c++11 ${COMMON_FLAGS}"
    ;;
  *)
    echo "-std=c++11 ${COMMON_FLAGS} -fno-exceptions -fno-rtti"
    ;;
  esac
}

# STAGES THE STATIC HALVES OF THE MinGW TOOLCHAIN RUNTIME IN THEIR OWN DIRECTORY
# AND ECHOES ITS PATH.
#
# THE PROBLEM: libc++, libunwind AND libwinpthread EACH SHIP AS BOTH A STATIC
# ARCHIVE (libc++.a) AND AN IMPORT LIBRARY FOR A DLL (libc++.dll.a ->
# libc++.dll), IN THE SAME TOOLCHAIN lib DIRECTORY. FOR -lc++ THE LINKER SEARCHES
# EACH -L DIRECTORY FOR libc++.dll.a BEFORE libc++.a, SO THE DLL IMPORT WINS AND
# THE OUTPUT ENDS UP DEPENDING ON libc++.dll / libwinpthread-1.dll - DLLs THAT
# ONLY EXIST INSIDE AN MSYS2 INSTALL, NOT ON THE MSVC MACHINES THIS PORT TARGETS.
#
# THE FIX: A DIRECTORY THAT HOLDS ONLY THE .a FILES. PREPENDED WITH -L IT IS
# SEARCHED FIRST, SO EVERY -lc++ / -lunwind / -lpthread / -lwinpthread ON THE
# LINE - THE COMPILER DRIVER'S OWN AND THE DOZENS INJECTED BY THE EXTERNAL
# LIBRARIES' .pc FILES ALIKE - RESOLVES TO THE STATIC ARCHIVE. IT NEVER REACHES
# THE TOOLCHAIN DIRECTORY WHERE THE IMPORT LIBRARY LIVES, SO THERE IS NO DLL
# DEPENDENCY AND, BECAUSE A SINGLE ARCHIVE SATISFIES EVERY REFERENCE, NO DUPLICATE
# SYMBOL EITHER.
#
# THIS IS SAFE BECAUSE EVERY DLL IN THE BUNDLE EXPOSES A C ABI: THE FFMPEG DLLs BY
# NATURE, libffmpegkit THROUGH ffmpegkit_c.h. NO C++ OBJECT OR EXCEPTION CROSSES A
# DLL BOUNDARY, SO A PRIVATE STATIC libc++ IN EACH DLL CANNOT COLLIDE WITH ANOTHER.
#
# libffmpegkit ITSELF DOES NOT USE THIS: libtool LINKS IT WITH -nostdlib AND ITS
# OWN ORDERED RUNTIME LIST, WHICH NEEDS THE DIFFERENT TREATMENT IN
# windows/configure.ac. THIS DIRECTORY IS FOR FFMPEG, WHOSE DRIVER DRIVEN LINK
# ONLY NEEDS THE STATIC ARCHIVE TO BE FOUND FIRST.
#
# --no-static-mingw-runtime SKIPS THIS; create_windows_bundle THEN SHIPS THE
# RUNTIME DLLs IN THE BUNDLE INSTEAD.
get_static_mingw_runtime_dir() {
  local RUNTIME_DIR="${FFMPEG_KIT_TMPDIR}/static-mingw-runtime-$(get_target_cpu)"

  # Rebuilt from scratch every call so a toolchain update cannot leave a stale
  # archive behind.
  rm -rf "${RUNTIME_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  mkdir -p "${RUNTIME_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  # libpthread.a is an alias of libwinpthread.a, and libc++.a already carries the
  # libc++abi objects, but both alternate spellings are staged so a -lpthread or
  # -lc++abi from any .pc file still resolves here rather than in the toolchain
  # directory.
  local ARCHIVE_NAME
  local ARCHIVE_PATH
  for ARCHIVE_NAME in libc++.a libc++abi.a libunwind.a libwinpthread.a libpthread.a; do
    ARCHIVE_PATH=$("${CC}" -print-file-name="${ARCHIVE_NAME}" 2>/dev/null)

    # -print-file-name echoes the name back unchanged when it cannot resolve it
    if [[ -n ${ARCHIVE_PATH} && -f ${ARCHIVE_PATH} ]]; then
      cp "${ARCHIVE_PATH}" "${RUNTIME_DIR}/${ARCHIVE_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
    else
      echo -e "\nWARNING: ${ARCHIVE_NAME} not found, its dll may stay a runtime dependency\n" 1>>"${BASEDIR}"/build.log 2>&1
    fi
  done

  echo "${RUNTIME_DIR}"
}

get_common_linked_libraries() {
  case $1 in
  ffmpeg)
    echo ""
    ;;
  ffmpeg-kit)
    echo ""
    ;;
  *)
    echo ""
    ;;
  esac
}

get_size_optimization_ldflags() {
  case $1 in
  ffmpeg)
    echo "-O2 -ffunction-sections -fdata-sections"
    ;;
  *)
    echo "-Os -ffunction-sections -fdata-sections"
    ;;
  esac
}

get_ldflags() {
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    local OPTIMIZATION_FLAGS="$(get_size_optimization_ldflags "$1")"
  else
    local OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi
  local COMMON_LINKED_LIBS=$(get_common_linked_libraries "$1")

  echo "${OPTIMIZATION_FLAGS} ${COMMON_LINKED_LIBS} ${EXTRA_LDFLAGS}"
}

# MSYS2 SHIPS TWO BUILDS OF meson. THE msys ONE RUNS ON msys/python AND REFUSES
# TO CONFIGURE A MinGW TARGET WITH "This python3 seems to be msys/python on
# MSYS2 Windows, but you are in a MinGW environment". ONLY THE MinGW BUILD WORKS
# HERE, AND "pacman -S meson" INSTALLS THE msys ONE, SO CHECK BEFORE CONFIGURING
# AND NAME THE PACKAGE THAT IS ACTUALLY NEEDED.
verify_meson() {
  local MESON_PATH
  MESON_PATH=$(command -v "${MESON:-meson}")

  if [[ -z ${MESON_PATH} ]]; then
    echo -e "\n(*) meson command not found." 1>&2
    echo -e "    Install it with: pacman -S ${MINGW_PACKAGE_PREFIX:-<mingw-prefix>}-meson\n" 1>&2
    echo -e "ERROR: meson not found on PATH\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  # msys PACKAGES INSTALL UNDER /usr/bin, MinGW PACKAGES UNDER THE ENVIRONMENT
  # PREFIX (/clangarm64/bin, /mingw64/bin, /ucrt64/bin, ...).
  case "${MESON_PATH}" in
  /usr/bin/*)
    echo -e "\n(*) ${MESON_PATH} is the msys build of meson, which cannot configure a MinGW target." 1>&2
    echo -e "    Install the MinGW build instead:" 1>&2
    echo -e "      pacman -S ${MINGW_PACKAGE_PREFIX:-<mingw-prefix>}-meson ${MINGW_PACKAGE_PREFIX:-<mingw-prefix>}-ninja\n" 1>&2
    echo -e "ERROR: msys meson detected at ${MESON_PATH}\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
    ;;
  esac
}

prepare_meson_build() {
  verify_meson || return 1

  CROSS_FILE="${BASEDIR}/src/${LIB_NAME}/package/crossfiles/${ARCH}-${FFMPEG_KIT_BUILD_TYPE}.meson"

  mkdir -p "$(dirname "${CROSS_FILE}")" || return 1
  create_mason_cross_file "${CROSS_FILE}" || return 1

  # ALWAYS CLEAN THE PREVIOUS BUILD
  rm -rf "${BUILD_DIR}" || return 1
}

# APPENDS THE .exe SUFFIX TO A PATH WHEN THE SUFFIXED FILE IS THE ONE THAT
# REALLY EXISTS ON DISK. DIRECTORIES AND ALREADY SUFFIXED PATHS ARE RETURNED
# UNCHANGED.
#
# THE MSYS2 SHELL RESOLVES "foo" TO "foo.exe" TRANSPARENTLY, SO command -v
# REPORTS THE PATH WITHOUT THE SUFFIX AND EVERY SHELL TEST ON IT SUCCEEDS.
# NATIVE WINDOWS TOOLS DO NOT ALL SHARE THAT BEHAVIOUR: CreateProcess APPENDS
# .exe WHEN IT LAUNCHES A PROGRAM, BUT A PLAIN FILESYSTEM STAT DOES NOT. cmake
# TRIPS OVER EXACTLY THAT DIFFERENCE - IT RUNS THE COMPILER FINE WHILE
# IDENTIFYING IT, THEN REJECTS CMAKE_C_COMPILER AS "not a full path to an
# existing compiler tool". APPENDING THE SUFFIX HERE KEEPS THE PATHS HANDED TO
# cmake AND meson NAMING A FILE THAT REALLY EXISTS.
#
# BECAUSE A TEST ON THE BARE NAME ALSO MATCHES THE "foo.exe" NEXT TO IT UNDER
# MSYS2, THE SUFFIXED NAME MUST BE TESTED EXPLICITLY TO TELL THE TWO APART.
#
# 1. path to a file or directory
add_exe_suffix() {
  local SOURCE_PATH="$1"

  if [[ ${SOURCE_PATH} != *.exe ]] && [[ ! -d "${SOURCE_PATH}" ]] && [[ -f "${SOURCE_PATH}.exe" ]]; then
    SOURCE_PATH="${SOURCE_PATH}.exe"
  fi

  echo "${SOURCE_PATH}"
}

# Resolves the first available command from a list of candidates and prints its
# absolute path. Prints nothing when none are found.
resolve_windows_tool() {
  local candidate
  for candidate in "$@"; do
    if [[ $(command_exists "${candidate}") -eq 0 ]]; then
      add_exe_suffix "$(command -v "${candidate}")"
      return
    fi
  done
}

set_toolchain_paths() {
  HOST=$(get_host)

  # COMPILERS: PREFER TRIPLET-PREFIXED WRAPPERS (llvm-mingw clang or GNU
  # MinGW-w64 gcc), THEN FALL BACK TO THE NATIVE MSYS2 COMPILER. THE BUILD IS
  # NATIVE, SO THE HOST ENVIRONMENT ALREADY TARGETS ${ARCH}.
  export CC=$(resolve_windows_tool "${HOST}-clang" "${HOST}-gcc" "clang" "gcc" "cc")
  export CXX=$(resolve_windows_tool "${HOST}-clang++" "${HOST}-g++" "clang++" "g++" "c++")

  # FAIL EARLY WITH AN ACTIONABLE MESSAGE WHEN THE MinGW-w64 COMPILER FOR ${ARCH}
  # IS NOT ON PATH.
  if [[ -z ${CC} || -z ${CXX} ]]; then
    local pkg="mingw-w64-clang-aarch64-toolchain (CLANGARM64 shell)"
    case ${ARCH} in
    x86-64)
      pkg="mingw-w64-x86_64-toolchain (MINGW64 shell) or mingw-w64-ucrt-x86_64-toolchain (UCRT64 shell)"
      ;;
    esac

    echo -e "\n(*) No MinGW-w64 compiler found on PATH for ${ARCH} (${HOST})." 1>&2
    echo -e "    Looked for: ${HOST}-clang, ${HOST}-gcc, clang, gcc, cc" 1>&2
    echo -e "    Install the toolchain from the matching MSYS2 shell, e.g.:" 1>&2
    echo -e "      pacman -S ${pkg}\n" 1>&2
    echo -e "ERROR: No C/C++ compiler resolved for ${ARCH}. CC='${CC}' CXX='${CXX}'\n" 1>>"${BASEDIR}"/build.log 2>&1
    exit 1
  fi

  # BINUTILS: llvm-mingw SHIPS llvm-* WRAPPERS UNDER THE TRIPLET PREFIX, GNU
  # MinGW-w64 SHIPS THE CLASSIC binutils NAMES.
  export AR=$(resolve_windows_tool "${HOST}-ar" "ar" "llvm-ar")
  export RANLIB=$(resolve_windows_tool "${HOST}-ranlib" "ranlib" "llvm-ranlib")
  export STRIP=$(resolve_windows_tool "${HOST}-strip" "strip" "llvm-strip")
  export NM=$(resolve_windows_tool "${HOST}-nm" "nm" "llvm-nm")
  export DLLTOOL=$(resolve_windows_tool "${HOST}-dlltool" "dlltool" "llvm-dlltool")
  export WINDRES=$(resolve_windows_tool "${HOST}-windres" "windres" "llvm-windres")

  # arm64 ASSEMBLY IS GAS .S, ASSEMBLED BY THE C COMPILER. x86_64 ASSEMBLY IS
  # NASM, WHICH FFMPEG DETECTS ON ITS OWN.
  export AS="${CC}"

  export INSTALL_PKG_CONFIG_DIR="${BASEDIR}"/prebuilt/$(get_build_directory)/pkgconfig

  if [ ! -d "${INSTALL_PKG_CONFIG_DIR}" ]; then
    mkdir -p "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  fi

  export ZLIB_PACKAGE_CONFIG_PATH="${INSTALL_PKG_CONFIG_DIR}/zlib.pc"
}

# MSYS2's autopoint (gettext-devel) cannot run because MSYS2 omits the gettext
# infrastructure archive (/usr/share/gettext/archive.dir.tar.xz). For any
# autotools project using AM_GNU_GETTEXT, autoreconf aborts at the autopoint
# step, which also prevents automake/libtoolize from installing the remaining
# aux files (config.guess, config.sub, install-sh, ltmain.sh, compile, missing).
#
# This seeds the gettext-provided aux files that autopoint would install -
# config.rpath (a required aux file) and, for a project with a po/ directory,
# po/Makefile.in.in - from the gettext data directory, which ships them
# separately from the missing archive. It then exports AUTOPOINT=true so the
# caller's autoreconf skips the broken autopoint; automake --add-missing and
# libtoolize still install the remaining aux files, and aclocal finds the
# gettext m4 macros in the system aclocal directory, so those need no seeding.
#
# Run this from the library source directory before autoreconf.
#
# 1. aux directory (AC_CONFIG_AUX_DIR value), relative to the current directory ["."]
seed_gettext_autotools_aux() {
  local AUX_DIR="${1:-.}"
  local AUTOPOINT_PATH GETTEXT_DATADIR="" candidate

  AUTOPOINT_PATH="$(command -v autopoint 2>>"${BASEDIR}"/build.log)"

  for candidate in \
    "${AUTOPOINT_PATH:+$(dirname "$(dirname "${AUTOPOINT_PATH}")")/share/gettext}" \
    "/usr/share/gettext" "/mingw64/share/gettext" "/ucrt64/share/gettext" "/clang64/share/gettext" "/clangarm64/share/gettext"; do
    if [[ -n "${candidate}" ]] && [[ -f "${candidate}/config.rpath" ]]; then
      GETTEXT_DATADIR="${candidate}"
      break
    fi
  done

  if [[ -z "${GETTEXT_DATADIR}" ]]; then
    echo -e "ERROR: Unable to locate the gettext data directory providing config.rpath. Install gettext-devel.\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  echo -e "INFO: Seeding gettext autotools aux files from ${GETTEXT_DATADIR} (MSYS2 autopoint workaround)\n" 1>>"${BASEDIR}"/build.log 2>&1

  mkdir -p "${AUX_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  overwrite_file "${GETTEXT_DATADIR}/config.rpath" "${AUX_DIR}/config.rpath" || return 1

  # Seed Makefile.in.in into every gettext message-catalog directory (each is
  # marked by a POTFILES.in and listed in AC_CONFIG_FILES, so config.status
  # generates its Makefile.in from the .in.in template). Some projects have more
  # than one - fontconfig has both po/ and po-conf/.
  if [[ -f "${GETTEXT_DATADIR}/po/Makefile.in.in" ]]; then
    local potfiles po_dir
    while IFS= read -r potfiles; do
      po_dir="$(dirname "${potfiles}")"
      if [[ ! -f "${po_dir}/Makefile.in.in" ]]; then
        overwrite_file "${GETTEXT_DATADIR}/po/Makefile.in.in" "${po_dir}/Makefile.in.in" || return 1
      fi
    done < <(find . -maxdepth 2 -type f -name POTFILES.in 2>>"${BASEDIR}"/build.log)
  fi

  export AUTOPOINT=true
}

# Installs the stock gettext po/Makevars.template that autopoint would provide.
#
# bootstrap scripts generate po/Makevars from this template, and it is one more
# file MSYS2's broken autopoint cannot install (see seed_gettext_autotools_aux
# for the rest). Its content is inert under --disable-nls; the generation step
# only needs the file to be readable.
#
# 1. po directory, relative to the current directory ["po"]
seed_gettext_po_makevars() {
  local PO_DIR="${1:-po}"

  mkdir -p "${PO_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  overwrite_file "${BASEDIR}/tools/patch/gettext/Makevars.template" "${PO_DIR}/Makevars.template" || return 1
}

get_clang_host() {
  echo "$(get_target_cpu)-w64-windows-gnu"
}

# CONVERTS AN MSYS2 POSIX PATH INTO THE MIXED WINDOWS FORM (F:/msys64/...).
#
# MSYS2 REWRITES POSIX PATHS THAT ARE PASSED TO A NATIVE WINDOWS PROGRAM AS
# COMMAND-LINE ARGUMENTS, BUT IT CANNOT REWRITE THE CONTENTS OF A FILE THAT THE
# PROGRAM OPENS ITSELF. THE MinGW BUILD OF meson IS A NATIVE WINDOWS PROGRAM AND
# READS ITS CROSS FILE DIRECTLY, SO EVERY PATH WRITTEN INTO THAT FILE HAS TO BE
# CONVERTED HERE OR meson FAILS WITH "[WinError 2] The system cannot find the
# file specified". THE MIXED FORM IS USED BECAUSE ITS FORWARD SLASHES NEED NO
# ESCAPING INSIDE THE CROSS FILE.
get_native_path() {
  local SOURCE_PATH="$1"

  if [[ -z ${SOURCE_PATH} ]]; then
    return 0
  fi

  # PREFER THE REAL .exe SO NATIVE TOOLS DO NOT HAVE TO GUESS THE EXTENSION.
  SOURCE_PATH=$(add_exe_suffix "${SOURCE_PATH}")

  if [[ $(command_exists "cygpath") -eq 0 ]]; then
    cygpath -m "${SOURCE_PATH}" 2>>"${BASEDIR}"/build.log
  else
    echo "${SOURCE_PATH}"
  fi
}

# Copy pkg-config files with cp-compatible arguments, then rewrite the absolute
# MSYS paths they contain for native Windows consumers such as MinGW Meson.
# MSYS only converts command-line arguments; paths read by pkg-config from a
# file otherwise reach clang unchanged as /home/... or /clangarm64/....
copy_and_update_path() {
  [[ $# -ge 2 ]] || return 1

  local DESTINATION="${!#}"
  local FILE

  cp "$@" || return 1

  if [[ -d ${DESTINATION} ]]; then
    for FILE in "${DESTINATION}"/*.pc; do
      [[ -f ${FILE} ]] || continue
      update_windows_package_config_path "${FILE}" || return 1
    done
  else
    update_windows_package_config_path "${DESTINATION}" || return 1
  fi
}

# Copy one pkg-config file, normalize its paths, and append options required by
# consumers of the installed library (for example, a static-library API macro).
copy_and_update_path_with_cflags() {
  [[ $# -eq 3 ]] || return 1

  local SOURCE="$1"
  local DESTINATION="$2"
  local EXTRA_CFLAGS="$3"
  local FILE="${DESTINATION}"

  copy_and_update_path "${SOURCE}" "${DESTINATION}" || return 1

  if [[ -d ${DESTINATION} ]]; then
    FILE="${DESTINATION}/$(basename "${SOURCE}")"
  fi

  [[ -f ${FILE} ]] || return 1
  grep -q '^Cflags:' "${FILE}" || return 1
  ${SED_INLINE} "/^Cflags:/ s|$| ${EXTRA_CFLAGS}|" "${FILE}" || return 1
}

update_windows_package_config_path() {
  local FILE="$1"
  local SOURCE_PATH
  local NATIVE_PATH
  local NATIVE_PATH_PLACEHOLDER="__FFMPEG_KIT_NATIVE_PATH__"

  [[ -f ${FILE} ]] || return 1

  # Build outputs live below BASEDIR; system dependencies live below the active
  # MinGW prefix. Replacing both roots also fixes absolute paths embedded in
  # Libs.private, not only the conventional prefix variable.
  for SOURCE_PATH in "${BASEDIR}" "${MINGW_PREFIX:-}"; do
    [[ -n ${SOURCE_PATH} ]] || continue
    NATIVE_PATH=$(get_native_path "${SOURCE_PATH}") || return 1
    if [[ ${SOURCE_PATH} != "${NATIVE_PATH}" ]]; then
      # Protect paths that were already normalized. The MSYS source path is a
      # suffix of its Windows form (F:/msys64/home/... contains /home/...), so
      # a direct replacement alone would add the Windows root more than once.
      ${SED_INLINE} "s|${NATIVE_PATH}|${NATIVE_PATH_PLACEHOLDER}|g" "${FILE}" || return 1
      ${SED_INLINE} "s|${SOURCE_PATH}|${NATIVE_PATH}|g" "${FILE}" || return 1
      ${SED_INLINE} "s|${NATIVE_PATH_PLACEHOLDER}|${NATIVE_PATH}|g" "${FILE}" || return 1
    fi
  done
}

create_mason_cross_file() {
  cat >"$1" <<EOF
[binaries]
c = '$(get_native_path "$CC")'
cpp = '$(get_native_path "$CXX")'
ar = '$(get_native_path "$AR")'
strip = '$(get_native_path "$STRIP")'
windres = '$(get_native_path "$WINDRES")'
pkg-config = 'pkg-config'

[properties]
has_function_printf = true

[host_machine]
system = '$(get_meson_target_host_family)'
cpu_family = '$(get_meson_target_cpu_family)'
cpu = '$(get_target_cpu)'
endian = 'little'

[built-in options]
default_library = 'static'
prefix = '$(get_native_path "${LIB_INSTALL_PREFIX}")'
EOF
}

install_pkg_config_file() {
  local FILE_NAME="$1"
  local SOURCE="${INSTALL_PKG_CONFIG_DIR}/${FILE_NAME}"
  local DESTINATION="${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}/${FILE_NAME}"
  local NATIVE_LIB_INSTALL_BASE
  local NATIVE_BUNDLE_PREFIX

  # DELETE OLD FILE
  rm -f "$DESTINATION" 2>>"${BASEDIR}"/build.log
  if [[ $? -ne 0 ]]; then
    exit 1
  fi

  # INSTALL THE NEW FILE
  copy_and_update_path "$SOURCE" "$DESTINATION" 2>>"${BASEDIR}"/build.log
  if [[ $? -ne 0 ]]; then
    exit 1
  fi

  # UPDATE PATHS
  NATIVE_LIB_INSTALL_BASE=$(get_native_path "${LIB_INSTALL_BASE}") || return 1
  NATIVE_BUNDLE_PREFIX=$(get_native_path "${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next") || return 1
  ${SED_INLINE} "s|${NATIVE_LIB_INSTALL_BASE}/ffmpeg-kit|${NATIVE_BUNDLE_PREFIX}|g" "$DESTINATION" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} "s|${NATIVE_LIB_INSTALL_BASE}/ffmpeg|${NATIVE_BUNDLE_PREFIX}|g" "$DESTINATION" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
}

create_ffmpegkit_package_config() {
  local FFMPEGKIT_VERSION="$1"
  local RUNTIME_DLLS="$2"

  # pkg-config has no concept of a runtime dll, so the list is exposed as a
  # custom variable: pkg-config --variable=runtime_dlls ffmpeg-kit-next. Empty
  # when the toolchain runtime was linked statically, which is the default.
  local RUNTIME_DLL_PATHS=""
  local RUNTIME_DLL
  for RUNTIME_DLL in ${RUNTIME_DLLS}; do
    RUNTIME_DLL_PATHS+="\${bindir}/${RUNTIME_DLL} "
  done

  cat >"${INSTALL_PKG_CONFIG_DIR}/ffmpeg-kit-next.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/ffmpeg-kit
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include
bindir=\${exec_prefix}/bin
runtime_dlls=${RUNTIME_DLL_PATHS}

Name: ffmpeg-kit-next
Description: FFmpeg for applications
Version: ${FFMPEGKIT_VERSION}

Libs: -L\${libdir} -lffmpegkit -lavutil
Requires: libavfilter, libswscale, libavformat, libavcodec, libswresample, libavutil
Cflags: -I\${includedir}
EOF
}

# THE MACHINE NAME lib.exe AND dlltool USE FOR THE TARGET ARCHITECTURE.
get_msvc_machine() {
  case ${ARCH} in
  arm64)
    echo "arm64"
    ;;
  x86-64)
    echo "x64"
    ;;
  esac
}

# LOCATES MSVC's lib.exe, WHICH IS NEVER ON PATH IN AN MSYS2 SHELL. THE VISUAL
# STUDIO LOCATOR IS ASKED FOR THE ACTIVE INSTALLATION AND THE NEWEST TOOLSET IN
# IT IS USED. lib.exe ONLY REWRITES A MODULE DEFINITION FILE INTO AN IMPORT
# LIBRARY, SO ANY HOST/TARGET FLAVOUR OF IT WORKS: -machine SELECTS THE OUTPUT
# ARCHITECTURE. PRINTS NOTHING WHEN VISUAL STUDIO IS NOT INSTALLED.
resolve_msvc_lib_tool() {
  if [[ $(command_exists "lib.exe") -eq 0 ]]; then
    command -v "lib.exe"
    return
  fi

  local VSWHERE="/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"
  if [[ ! -f ${VSWHERE} ]]; then
    return
  fi

  local VS_PATH
  VS_PATH=$("${VSWHERE}" -latest -products '*' -property installationPath 2>/dev/null | tr -d '\r')
  if [[ -z ${VS_PATH} ]]; then
    return
  fi

  local CANDIDATE
  CANDIDATE=$(ls "$(cygpath -u "${VS_PATH}")"/VC/Tools/MSVC/*/bin/Host*/*/lib.exe 2>/dev/null | sort | tail -1)
  if [[ -n ${CANDIDATE} ]]; then
    echo "${CANDIDATE}"
  fi
}

# CREATES THE MSVC IMPORT LIBRARY AND MODULE DEFINITION FILE OF libffmpegkit.
#
# libffmpegkit EXPORTS ONLY THE FLAT C API DECLARED IN ffmpegkit_c.h, SO MSVC AND
# clang-cl CONSUMERS - FLUTTER WINDOWS AND REACT NATIVE WINDOWS AMONG THEM - CAN
# LINK AGAINST IT EVEN THOUGH IT IS BUILT WITH MinGW-w64. THOSE TOOLCHAINS LINK
# AGAINST A .lib IMPORT LIBRARY RATHER THAN THE .dll.a libtool PRODUCES.
#
# THE EXPORT LIST COMES FROM THE MODULE DEFINITION FILE libtool ALREADY WRITES
# FOR THE -export-symbols-regex IN src/Makefile.am, SO IT CANNOT DRIFT FROM WHAT
# THE DLL ACTUALLY EXPORTS. THE .lib IS THEN BUILT THE SAME WAY FFMPEG BUILDS ITS
# OWN (SEE THE mingw32 BLOCK IN src/ffmpeg/configure): lib.exe WHEN VISUAL STUDIO
# IS INSTALLED, dlltool OTHERWISE.
create_ffmpegkit_msvc_import_library() {
  local BUNDLE_BIN_DIRECTORY="$1"
  local BUNDLE_LIB_DIRECTORY="$2"

  local DLL_PATH
  DLL_PATH=$(ls "${BUNDLE_BIN_DIRECTORY}"/libffmpegkit-*.dll 2>/dev/null | head -1)
  if [[ -z ${DLL_PATH} ]]; then
    echo -e "\nERROR: libffmpegkit dll not found under ${BUNDLE_BIN_DIRECTORY}\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi
  local DLL_NAME
  DLL_NAME=$(basename "${DLL_PATH}")

  local LIBTOOL_DEF_PATH
  LIBTOOL_DEF_PATH=$(ls "${BASEDIR}"/windows/src/.libs/libffmpegkit-*.dll.def 2>/dev/null | head -1)
  if [[ -z ${LIBTOOL_DEF_PATH} ]]; then
    echo -e "\nERROR: libffmpegkit module definition file not found under ${BASEDIR}/windows/src/.libs\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  local DEF_PATH="${BUNDLE_LIB_DIRECTORY}/libffmpegkit.def"
  local LIB_PATH="${BUNDLE_LIB_DIRECTORY}/ffmpegkit.lib"

  # libtool's file carries no LIBRARY statement, and without one the import
  # library would name the dll after the definition file. Adding it keeps the
  # bundled file name (libffmpegkit.def) independent of the dll version suffix.
  {
    echo "LIBRARY ${DLL_NAME}"
    grep -v '^LIBRARY' "${LIBTOOL_DEF_PATH}"
  } >"${DEF_PATH}" 2>>"${BASEDIR}"/build.log || return 1

  rm -f "${LIB_PATH}" 1>>"${BASEDIR}"/build.log 2>&1

  local MACHINE
  MACHINE=$(get_msvc_machine)

  local LIB_TOOL
  LIB_TOOL=$(resolve_msvc_lib_tool)
  if [[ -n ${LIB_TOOL} ]]; then
    # MSYS2 rewrites anything that looks like a path in an argument, which would
    # corrupt the -def:/-out: switches lib.exe expects
    (
      cd "${BUNDLE_LIB_DIRECTORY}" || exit 1
      MSYS2_ARG_CONV_EXCL='*' "${LIB_TOOL}" -nologo "-machine:${MACHINE}" \
        "-def:libffmpegkit.def" "-out:ffmpegkit.lib"
    ) 1>>"${BASEDIR}"/build.log 2>&1
  else
    echo -e "INFO: lib.exe not found, falling back to dlltool for ffmpegkit.lib\n" 1>>"${BASEDIR}"/build.log 2>&1
    "${DLLTOOL}" -m "${MACHINE}" -d "${DEF_PATH}" -l "${LIB_PATH}" -D "${DLL_NAME}" 1>>"${BASEDIR}"/build.log 2>&1
  fi

  if [[ ! -f ${LIB_PATH} ]]; then
    echo -e "\nERROR: failed to create ${LIB_PATH}\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  # lib.exe drops an export file next to the import library that nothing
  # consumes
  rm -f "${BUNDLE_LIB_DIRECTORY}/ffmpegkit.exp" 1>>"${BASEDIR}"/build.log 2>&1

  return 0
}

# COPIES THE MinGW TOOLCHAIN RUNTIME DLLs INTO THE BUNDLE AND PRINTS THEIR FILE
# NAMES, ONE PER LINE. PRINTS NOTHING WHEN THE RUNTIME IS LINKED STATICALLY,
# WHICH IS THE DEFAULT.
#
# THE LIST IS DERIVED FROM WHAT THE BUILT DLLs ACTUALLY IMPORT RATHER THAN
# HARDCODED, SO IT STAYS CORRECT ACROSS MSYS2 ENVIRONMENTS: CLANGARM64 AND
# CLANG64 NEED libc++.dll, MINGW64 AND UCRT64 NEED libstdc++-6.dll AND
# libgcc_s_seh-1.dll INSTEAD. SYSTEM DLLs (KERNEL32, THE api-ms-win-crt-* UCRT
# FACADES, WHICH SHIP WITH WINDOWS) AND THE BUNDLE'S OWN DLLs ARE SKIPPED.
copy_mingw_runtime_libraries() {
  local BUNDLE_BIN_DIRECTORY="$1"

  if [[ -z ${NO_STATIC_MINGW_RUNTIME} ]]; then
    return 0
  fi

  local TOOLCHAIN_BIN_DIRECTORY
  TOOLCHAIN_BIN_DIRECTORY=$(dirname "${CC}")

  local OBJDUMP_TOOL
  OBJDUMP_TOOL=$(resolve_windows_tool "${HOST}-objdump" "objdump" "llvm-objdump")
  if [[ -z ${OBJDUMP_TOOL} ]]; then
    echo -e "\nERROR: objdump not found, cannot resolve the runtime dependencies of the bundle\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  # Repeat until nothing new turns up: a runtime dll can itself depend on
  # another one (libc++.dll needs libunwind.dll in some MSYS2 environments).
  local ADDED=1
  local COPIED=""
  while [[ ${ADDED} -eq 1 ]]; do
    ADDED=0

    local DLL_FILE
    local DEPENDENCY
    for DLL_FILE in "${BUNDLE_BIN_DIRECTORY}"/*.dll; do
      for DEPENDENCY in $("${OBJDUMP_TOOL}" -p "${DLL_FILE}" 2>/dev/null | grep -oiE "DLL Name: [A-Za-z0-9_+.-]+\.dll" | sed 's/^DLL Name: //'); do

        # already in the bundle, either shipped by us or copied on a previous pass
        if [[ -f "${BUNDLE_BIN_DIRECTORY}/${DEPENDENCY}" ]]; then
          continue
        fi

        # only ever take a dll that sits in the toolchain, which excludes every
        # system dll without having to enumerate them
        if [[ ! -f "${TOOLCHAIN_BIN_DIRECTORY}/${DEPENDENCY}" ]]; then
          continue
        fi

        cp -P "${TOOLCHAIN_BIN_DIRECTORY}/${DEPENDENCY}" "${BUNDLE_BIN_DIRECTORY}" 2>>"${BASEDIR}"/build.log || return 1
        COPIED+="${DEPENDENCY}"$'\n'
        ADDED=1
      done
    done
  done

  printf "%s" "${COPIED}"
  return 0
}

# COPIES THE LICENSES OF THE MinGW TOOLCHAIN RUNTIME INTO THE BUNDLE.
#
# THIS IS REQUIRED WHETHER THE RUNTIME IS LINKED STATICALLY OR SHIPPED AS DLLs:
# BOTH ARE REDISTRIBUTION. libc++ AND libunwind ARE Apache-2.0 WITH THE LLVM
# EXCEPTION AND libwinpthread IS UNDER THE MinGW-w64 TERMS; ALL THREE ASK FOR
# THE LICENSE TEXT TO TRAVEL WITH THE BINARY.
#
# libunwind IS THE STACK UNWINDER libc++ CALLS FOR C++ EXCEPTIONS. IN THE STATIC
# BUILD IT IS COMPILED INTO libffmpegkit-<v>.dll (libtool's -lunwind WOULD
# OTHERWISE PULL BACK IN libunwind.dll); IN THE --no-static-mingw-runtime BUILD
# IT TRAVELS INSIDE THE SHIPPED libc++.dll. EITHER WAY ITS CODE IS REDISTRIBUTED,
# SO ITS LICENSE SHIPS IN BOTH MODES.
copy_mingw_runtime_licenses() {
  local LICENSE_DIRECTORY="$1"

  local TOOLCHAIN_LICENSE_DIRECTORY
  TOOLCHAIN_LICENSE_DIRECTORY="$(dirname "$(dirname "${CC}")")/share/licenses"

  # <license directory under share/licenses>:<name in the bundle>
  local ENTRY
  local SOURCE_DIRECTORY
  local TARGET_NAME
  local LICENSE_FILE
  for ENTRY in "libc++:libcxx" "libwinpthread:libwinpthread" "libunwind:libunwind"; do
    SOURCE_DIRECTORY="${TOOLCHAIN_LICENSE_DIRECTORY}/${ENTRY%%:*}"
    TARGET_NAME="${ENTRY##*:}"

    if [[ ! -d ${SOURCE_DIRECTORY} ]]; then
      echo -e "INFO: no license directory for ${TARGET_NAME} under ${TOOLCHAIN_LICENSE_DIRECTORY}, skipped\n" 1>>"${BASEDIR}"/build.log 2>&1
      continue
    fi

    # the packages name the file LICENSE or COPYING depending on upstream
    LICENSE_FILE=$(ls "${SOURCE_DIRECTORY}"/LICENSE* "${SOURCE_DIRECTORY}"/COPYING* 2>/dev/null | head -1)
    if [[ -z ${LICENSE_FILE} ]]; then
      echo -e "INFO: no license file for ${TARGET_NAME} under ${SOURCE_DIRECTORY}, skipped\n" 1>>"${BASEDIR}"/build.log 2>&1
      continue
    fi

    cp "${LICENSE_FILE}" "${LICENSE_DIRECTORY}/license_${TARGET_NAME}.txt" 2>>"${BASEDIR}"/build.log || return 1
    echo -e "DEBUG: Copied the license file of ${TARGET_NAME} successfully\n" 1>>"${BASEDIR}"/build.log 2>&1
  done

  return 0
}

# CREATES THE CMAKE PACKAGE CONFIGURATION THE MSVC CONSUMERS DISCOVER THE BUNDLE
# WITH.
#
# THE BUNDLE SHIPS TWO IMPORT LIBRARY FORMATS AGAINST THE SAME DLLs AND ONE
# DISCOVERY MECHANISM FOR EACH. MinGW-w64 READS lib/pkgconfig/ffmpeg-kit-next.pc,
# WHOSE "Libs: -L${libdir} -lffmpegkit" LINE IS GNU LINKER SYNTAX link.exe DOES
# NOT UNDERSTAND. MSVC AND clang-cl THEREFORE GET THIS PACKAGE CONFIG INSTEAD,
# WHICH POINTS AT THE .lib FILES create_ffmpegkit_msvc_import_library() AND THE
# FFMPEG BUILD PRODUCE.
#
# THE FILE IS WRITTEN RELOCATABLE: IT DERIVES THE BUNDLE PREFIX FROM ITS OWN
# LOCATION RATHER THAN EMBEDDING THE BUILD MACHINE PATH, SO THE BUNDLE CAN BE
# MOVED OR COPIED TO ANOTHER MACHINE. THE .pc FILES CANNOT DO THAT AND ARE PATH
# REWRITTEN BY install_pkg_config_file() INSTEAD.
#
# EVERY DLL IN THE BUNDLE GETS AN IMPORTED TARGET, NOT ONLY libffmpegkit. THE
# IMPORTED_LOCATION ON EACH IS WHAT LETS A CONSUMER COPY THE WHOLE RUNTIME WITH
# $<TARGET_RUNTIME_DLLS:...>, AND THE FFMPEG TARGETS MIRROR THE "Requires:" LINE
# OF THE .pc FILE SO BOTH ABIs LINK THE SAME SET.
create_ffmpegkit_cmake_package_config() {
  local FFMPEGKIT_VERSION="$1"
  local BUNDLE_BIN_DIRECTORY="$2"
  local BUNDLE_LIB_DIRECTORY="$3"
  local BUNDLE_CMAKE_DIRECTORY="$4"
  local RUNTIME_DLLS="$5"

  local CONFIG_PATH="${BUNDLE_CMAKE_DIRECTORY}/ffmpeg-kit-next-config.cmake"
  local VERSION_PATH="${BUNDLE_CMAKE_DIRECTORY}/ffmpeg-kit-next-config-version.cmake"

  # THE FFMPEG COMPONENTS IN LINK ORDER.
  #
  # avdevice IS LISTED EVEN THOUGH ffmpeg-kit-next.pc LEAVES IT OUT OF ITS
  # "Requires:" LINE: scripts/windows/ffmpeg-kit.sh LINKS THE LIBRARY WITH
  # -lavdevice, SO libffmpegkit-<v>.dll REALLY DOES IMPORT avdevice-<v>.dll AND
  # WILL NOT LOAD WITHOUT IT. A pkg-config CONSUMER NEVER NOTICES, BECAUSE THE
  # LOADER FINDS THE DLL NEXT TO THE OTHERS IN bin/, BUT $<TARGET_RUNTIME_DLLS>
  # ONLY SEES WHAT THE LINK INTERFACE DECLARES - LEAVING avdevice OUT MAKES CMAKE
  # DEPLOY AN INCOMPLETE RUNTIME AND THE APPLICATION FAILS TO START WITH
  # STATUS_DLL_NOT_FOUND.
  local FFMPEG_COMPONENTS="avdevice avfilter swscale avformat avcodec swresample avutil"

  # RESOLVES THE VERSIONED DLL FILE NAME THAT BELONGS TO AN IMPORT LIBRARY.
  # FFMPEG EMITS avcodec.lib NEXT TO avcodec-62.dll WHILE LIBTOOL EMITS
  # ffmpegkit.lib NEXT TO libffmpegkit-12.dll, SO BOTH SPELLINGS ARE TRIED.
  local COMPONENT
  local DLL_NAME
  local DLL_PATH
  local ADD_LIBRARY_LINES=""

  for COMPONENT in ${FFMPEG_COMPONENTS} ffmpegkit; do
    if [[ ! -f ${BUNDLE_LIB_DIRECTORY}/${COMPONENT}.lib ]]; then
      echo -e "\nERROR: ${COMPONENT}.lib not found under ${BUNDLE_LIB_DIRECTORY}\n" 1>>"${BASEDIR}"/build.log 2>&1
      return 1
    fi

    DLL_PATH=$(ls "${BUNDLE_BIN_DIRECTORY}/${COMPONENT}"-*.dll "${BUNDLE_BIN_DIRECTORY}/lib${COMPONENT}"-*.dll 2>/dev/null | head -1)
    if [[ -z ${DLL_PATH} ]]; then
      echo -e "\nERROR: runtime dll of ${COMPONENT} not found under ${BUNDLE_BIN_DIRECTORY}\n" 1>>"${BASEDIR}"/build.log 2>&1
      return 1
    fi
    DLL_NAME=$(basename "${DLL_PATH}")

    ADD_LIBRARY_LINES+="_ffmpeg_kit_next_add_library(${COMPONENT} \"${COMPONENT}.lib\" \"${DLL_NAME}\")"$'\n'
  done

  # THE FFMPEG TARGETS ffmpegkit LINKS AGAINST, AS A CMAKE LIST.
  local INTERFACE_LINK_LIBRARIES=""
  for COMPONENT in ${FFMPEG_COMPONENTS}; do
    INTERFACE_LINK_LIBRARIES+="        ffmpeg-kit-next::${COMPONENT}"$'\n'
  done

  # THE MinGW RUNTIME DLLs, WHEN THEY ARE SHIPPED RATHER THAN LINKED STATICALLY.
  # THEY CANNOT BE IMPORTED TARGETS BECAUSE THEY HAVE NO IMPORT LIBRARY TO LINK
  # AGAINST, AND $<TARGET_RUNTIME_DLLS> ONLY WALKS THE LINK CLOSURE, SO THEY ARE
  # EXPOSED AS A PLAIN LIST OF PATHS FOR THE CONSUMER TO COPY ALONGSIDE IT.
  local RUNTIME_DLL_ENTRIES=""
  local RUNTIME_DLL
  for RUNTIME_DLL in ${RUNTIME_DLLS}; do
    RUNTIME_DLL_ENTRIES+="    \"\${FFMPEG_KIT_NEXT_BINARY_DIR}/${RUNTIME_DLL}\""$'\n'
  done

  cat >"${CONFIG_PATH}" <<EOF 2>>"${BASEDIR}"/build.log || return 1
# FFmpegKitNext package configuration for MSVC and clang-cl consumers.
#
# Generated by create_ffmpegkit_cmake_package_config(). Do not edit.
#
# Usage:
#
#   list(APPEND CMAKE_PREFIX_PATH "<path-to-bundle>")
#   find_package(ffmpeg-kit-next ${FFMPEGKIT_VERSION} REQUIRED CONFIG)
#   target_link_libraries(<your-target> PRIVATE ffmpeg-kit-next::ffmpegkit)
#
# MinGW-w64 consumers use lib/pkgconfig/ffmpeg-kit-next.pc instead; the import
# libraries named here are the MSVC format ones.

set(FFMPEG_KIT_NEXT_VERSION "${FFMPEGKIT_VERSION}")
set(FFMPEG_KIT_NEXT_ARCHITECTURE "$(get_msvc_machine)")

# The bundle prefix is derived from this file's own location -
# <prefix>/lib/cmake/ffmpeg-kit-next - so the bundle stays relocatable.
get_filename_component(FFMPEG_KIT_NEXT_PREFIX "\${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(FFMPEG_KIT_NEXT_INCLUDE_DIR "\${FFMPEG_KIT_NEXT_PREFIX}/include")
set(FFMPEG_KIT_NEXT_LIBRARY_DIR "\${FFMPEG_KIT_NEXT_PREFIX}/lib")
set(FFMPEG_KIT_NEXT_BINARY_DIR "\${FFMPEG_KIT_NEXT_PREFIX}/bin")

if(NOT EXISTS "\${FFMPEG_KIT_NEXT_INCLUDE_DIR}/ffmpegkit_c.h")
    set(ffmpeg-kit-next_FOUND FALSE)
    set(ffmpeg-kit-next_NOT_FOUND_MESSAGE
        "The FFmpegKitNext bundle at \${FFMPEG_KIT_NEXT_PREFIX} is incomplete: include/ffmpegkit_c.h is missing.")
    return()
endif()

# The bundle is built for a single 64 bit architecture. Catch the -A Win32 case
# here rather than in a link error nobody can read.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ffmpeg-kit-next_FOUND FALSE)
    set(ffmpeg-kit-next_NOT_FOUND_MESSAGE
        "This FFmpegKitNext bundle is \${FFMPEG_KIT_NEXT_ARCHITECTURE} only and cannot be linked into a 32 bit target.")
    return()
endif()

# Each DLL becomes a SHARED IMPORTED target. IMPORTED_IMPLIB is the .lib
# link.exe consumes; IMPORTED_LOCATION is the DLL the loader needs at runtime and
# is what makes \$<TARGET_RUNTIME_DLLS:...> able to copy the runtime.
macro(_ffmpeg_kit_next_add_library _component _implib _dll)
    if(NOT TARGET ffmpeg-kit-next::\${_component})
        add_library(ffmpeg-kit-next::\${_component} SHARED IMPORTED)
        set_target_properties(ffmpeg-kit-next::\${_component} PROPERTIES
            IMPORTED_IMPLIB "\${FFMPEG_KIT_NEXT_LIBRARY_DIR}/\${_implib}"
            IMPORTED_LOCATION "\${FFMPEG_KIT_NEXT_BINARY_DIR}/\${_dll}"
            INTERFACE_INCLUDE_DIRECTORIES "\${FFMPEG_KIT_NEXT_INCLUDE_DIR}")
    endif()
endmacro()

${ADD_LIBRARY_LINES}
# libffmpegkit exports the flat C API declared in ffmpegkit_c.h and links the
# FFmpeg libraries itself, but a consumer that also calls libav* directly gets
# them transitively here, the same set the .pc file lists under "Requires:".
set_property(TARGET ffmpeg-kit-next::ffmpegkit APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES
${INTERFACE_LINK_LIBRARIES})

# DLLs that are not part of the link closure and therefore never appear in
# \$<TARGET_RUNTIME_DLLS:...>. Empty when the toolchain runtime was linked
# statically, which is the default; populated by --no-static-mingw-runtime.
# Copy them next to the executable together with the runtime dlls, e.g.
#
#   add_custom_command(TARGET app POST_BUILD
#       COMMAND \${CMAKE_COMMAND} -E copy_if_different
#               \$<TARGET_RUNTIME_DLLS:app> \${FFMPEG_KIT_NEXT_RUNTIME_DLLS}
#               \$<TARGET_FILE_DIR:app>
#       COMMAND_EXPAND_LISTS)
set(FFMPEG_KIT_NEXT_RUNTIME_DLLS
${RUNTIME_DLL_ENTRIES})

set(ffmpeg-kit-next_FOUND TRUE)
EOF

  cat >"${VERSION_PATH}" <<EOF 2>>"${BASEDIR}"/build.log || return 1
# FFmpegKitNext package version file. Generated by
# create_ffmpegkit_cmake_package_config(). Do not edit.

set(PACKAGE_VERSION "${FFMPEGKIT_VERSION}")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_VERSION VERSION_EQUAL PACKAGE_FIND_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()

# The bundle is architecture specific and 64 bit only.
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()
EOF

  return 0
}

create_windows_bundle() {
  set_toolchain_paths ""

  local FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/include"
  local FFMPEG_KIT_BUNDLE_LIB_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib"
  local FFMPEG_KIT_BUNDLE_BIN_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/bin"
  local FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib/pkgconfig"
  local FFMPEG_KIT_BUNDLE_CMAKE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib/cmake/ffmpeg-kit-next"
  local FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/licenses"

  initialize_folder "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_CMAKE_DIRECTORY}"
  initialize_folder "${FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY}"

  # COPY HEADERS
  cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/include/* "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg/include/* "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}" 2>>"${BASEDIR}"/build.log

  # COPY RUNTIME DLLs (bin) AND IMPORT LIBRARIES (lib)
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg/bin/*.dll "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/bin/*.dll "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg/lib/*.dll.a "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/lib/*.dll.a "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" 2>>"${BASEDIR}"/build.log

  # COPY MSVC IMPORT LIBRARIES AND MODULE DEFINITION FILES OF FFMPEG (lib)
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg/bin/*.lib "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  cp -P "${LIB_INSTALL_BASE}"/ffmpeg/lib/*.def "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" 2>>"${BASEDIR}"/build.log

  # CREATE THE MSVC IMPORT LIBRARY AND MODULE DEFINITION FILE OF libffmpegkit (lib)
  create_ffmpegkit_msvc_import_library "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}" "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" || return 1

  # COPY THE MinGW TOOLCHAIN RUNTIME DLLs (bin). ONLY DOES ANYTHING UNDER
  # --no-static-mingw-runtime; BY DEFAULT THE RUNTIME IS INSIDE THE DLLs ALREADY.
  local FFMPEG_KIT_RUNTIME_DLLS
  FFMPEG_KIT_RUNTIME_DLLS=$(copy_mingw_runtime_libraries "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}") || return 1

  # CREATE THE CMAKE PACKAGE CONFIGURATION MSVC CONSUMERS DISCOVER THE BUNDLE
  # WITH (lib/cmake). RUNS AFTER THE IMPORT LIBRARIES AND THE DLLs ARE IN PLACE
  # BECAUSE IT RESOLVES THE VERSIONED DLL FILE NAMES FROM THE BUNDLE ITSELF.
  create_ffmpegkit_cmake_package_config "$(get_ffmpeg_kit_version)" \
    "${FFMPEG_KIT_BUNDLE_BIN_DIRECTORY}" "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" \
    "${FFMPEG_KIT_BUNDLE_CMAKE_DIRECTORY}" "${FFMPEG_KIT_RUNTIME_DLLS}" || return 1

  # REGENERATE ffmpeg-kit-next.pc SO THAT ITS runtime_dlls VARIABLE MATCHES WHAT
  # WAS ACTUALLY COPIED. THE ffmpeg-kit BUILD WROTE IT ONCE ALREADY, BEFORE THE
  # BUNDLE EXISTED AND THEREFORE BEFORE THE LIST WAS KNOWN.
  create_ffmpegkit_package_config "$(get_ffmpeg_kit_version)" "${FFMPEG_KIT_RUNTIME_DLLS}" || return 1

  install_pkg_config_file "libavformat.pc"
  install_pkg_config_file "libswresample.pc"
  install_pkg_config_file "libswscale.pc"
  install_pkg_config_file "libavdevice.pc"
  install_pkg_config_file "libavfilter.pc"
  install_pkg_config_file "libavcodec.pc"
  install_pkg_config_file "libavutil.pc"
  install_pkg_config_file "ffmpeg-kit-next.pc"

  # COPY EXTERNAL LIBRARY LICENSES
  LICENSE_BASEDIR="${FFMPEG_KIT_BUNDLE_LICENSE_DIRECTORY}"
  rm -f "${LICENSE_BASEDIR}"/*.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  for library in $(get_common_library_indexes); do
    if [[ ${ENABLED_LIBRARIES[$library]} -eq 1 ]]; then
      ENABLED_LIBRARY=$(get_library_name ${library} | sed 's/-/_/g')
      LICENSE_FILE="${LICENSE_BASEDIR}/license_${ENABLED_LIBRARY}.txt"

      RC=$(copy_external_library_license_file ${library} "${LICENSE_FILE}")

      if [[ ${RC} -ne 0 ]]; then
        echo -e "ERROR: Failed to copy the license file of ${ENABLED_LIBRARY}\n" 1>>"${BASEDIR}"/build.log 2>&1
        exit 1
      fi

      echo -e "DEBUG: Copied the license file of ${ENABLED_LIBRARY} successfully\n" 1>>"${BASEDIR}"/build.log 2>&1
    fi
  done

  # COPY LIBRARY LICENSES
  if [[ ${GPL_ENABLED} == "yes" ]]; then
    cp "${BASEDIR}"/tools/license/LICENSE.GPLv3 "${LICENSE_BASEDIR}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  else
    cp "${BASEDIR}"/LICENSE "${LICENSE_BASEDIR}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1
  fi

  cp "${BASEDIR}"/tools/source/SOURCE "${LICENSE_BASEDIR}"/source.txt 1>>"${BASEDIR}"/build.log 2>&1 || exit 1

  # COPY THE LICENSES OF THE MinGW TOOLCHAIN RUNTIME. NEEDED WHETHER IT IS LINKED
  # STATICALLY OR SHIPPED AS DLLs, SINCE BOTH REDISTRIBUTE IT.
  copy_mingw_runtime_licenses "${LICENSE_BASEDIR}" || exit 1

  echo -e "DEBUG: Copied the ffmpeg-kit license successfully\n" 1>>"${BASEDIR}"/build.log 2>&1
}

create_chromaprint_package_config() {
  local CHROMAPRINT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libchromaprint.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/chromaprint
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: chromaprint
Description: Audio fingerprint library
URL: http://acoustid.org/chromaprint
Version: ${CHROMAPRINT_VERSION}
Libs: -L\${libdir} -lchromaprint -lstdc++
Cflags: -I\${includedir} -DCHROMAPRINT_NODLL
EOF
}

create_fontconfig_package_config() {
  local FONTCONFIG_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/fontconfig.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/fontconfig
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include
sysconfdir=\${prefix}/etc
localstatedir=\${prefix}/var
PACKAGE=fontconfig
confdir=\${sysconfdir}/fonts
cachedir=\${localstatedir}/cache/\${PACKAGE}

Name: Fontconfig
Description: Font configuration and customization library
Version: ${FONTCONFIG_VERSION}
Requires:  freetype2 >= 21.0.15, expat >= 2.2.0, libiconv
Requires.private:
Libs: -L\${libdir} -lfontconfig
Libs.private:
Cflags: -I\${includedir}
EOF
}

create_freetype_package_config() {
  local FREETYPE_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/freetype2.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/freetype
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: FreeType 2
URL: https://freetype.org
Description: A free, high-quality, and portable font engine.
Version: ${FREETYPE_VERSION}
Requires: libpng
Requires.private: zlib
Libs: -L\${libdir} -lfreetype
Libs.private:
Cflags: -I\${includedir}/freetype2
EOF
}

create_giflib_package_config() {
  local GIFLIB_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/giflib.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/giflib
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: giflib
Description: gif library
Version: ${GIFLIB_VERSION}

Requires:
Libs: -L\${libdir} -lgif
Cflags: -I\${includedir}
EOF
}

create_gmp_package_config() {
  local GMP_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/gmp.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/gmp
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: gmp
Description: gnu mp library
Version: ${GMP_VERSION}

Requires:
Libs: -L\${libdir} -lgmp
Cflags: -I\${includedir}
EOF
}

create_gnutls_package_config() {
  local GNUTLS_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/gnutls.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/gnutls
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: gnutls
Description: GNU TLS Implementation

Version: ${GNUTLS_VERSION}
Requires: nettle, hogweed, zlib
Cflags: -I\${includedir}
Libs: -L\${libdir} -lgnutls
Libs.private: -lgmp -ladvapi32 -lcrypt32 -lncrypt -lbcrypt -lws2_32
EOF
}

create_libaom_package_config() {
  local AOM_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/aom.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libaom
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: aom
Description: AV1 codec library v${AOM_VERSION}.
Version: ${AOM_VERSION}

Requires:
Libs: -L\${libdir} -laom -lm
Cflags: -I\${includedir}
EOF
}

create_libiconv_package_config() {
  local LIB_ICONV_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libiconv.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libiconv
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libiconv
Description: Character set conversion library
Version: ${LIB_ICONV_VERSION}

Requires:
Libs: -L\${libdir} -liconv -lcharset
Cflags: -I\${includedir}
EOF
}

create_libmp3lame_package_config() {
  local LAME_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libmp3lame.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/lame
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libmp3lame
Description: lame mp3 encoder library
Version: ${LAME_VERSION}

Requires:
Libs: -L\${libdir} -lmp3lame
Cflags: -I\${includedir}
EOF
}

create_libvorbis_package_config() {
  local LIBVORBIS_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/vorbis.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libvorbis
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: vorbis
Description: vorbis is the primary Ogg Vorbis library
Version: ${LIBVORBIS_VERSION}

Requires: ogg
Libs: -L\${libdir} -lvorbis -lm
Cflags: -I\${includedir}
EOF

  cat >"${INSTALL_PKG_CONFIG_DIR}/vorbisenc.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libvorbis
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: vorbisenc
Description: vorbisenc is a library that provides a convenient API for setting up an encoding environment using libvorbis
Version: ${LIBVORBIS_VERSION}

Requires: vorbis
Conflicts:
Libs: -L\${libdir} -lvorbisenc
Cflags: -I\${includedir}
EOF

  cat >"${INSTALL_PKG_CONFIG_DIR}/vorbisfile.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libvorbis
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: vorbisfile
Description: vorbisfile is a library that provides a convenient high-level API for decoding and basic manipulation of all Vorbis I audio streams
Version: ${LIBVORBIS_VERSION}

Requires: vorbis
Conflicts:
Libs: -L\${libdir} -lvorbisfile
Cflags: -I\${includedir}
EOF
}

create_libxml2_package_config() {
  local LIBXML2_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libxml-2.0.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/libxml2
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include
modules=1

Name: libXML
Version: ${LIBXML2_VERSION}
Description: libXML library version2.
Requires: libiconv
Requires.private: zlib
Libs: -L\${libdir} -lxml2
Libs.private:   -lm
Cflags: -I\${includedir} -I\${includedir}/libxml2 -DLIBXML_STATIC
EOF
}

create_snappy_package_config() {
  local SNAPPY_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/snappy.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/snappy
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: snappy
Description: a fast compressor/decompressor
Version: ${SNAPPY_VERSION}

Requires:
Libs: -L\${libdir} -lsnappy -lstdc++
Cflags: -I\${includedir}
EOF
}

create_soxr_package_config() {
  local SOXR_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/soxr.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/soxr
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: soxr
Description: High quality, one-dimensional sample-rate conversion library
Version: ${SOXR_VERSION}

Requires:
Libs: -L\${libdir} -lsoxr
Cflags: -I\${includedir}
EOF
}

create_srt_package_config() {
  local SRT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/srt.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/srt
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: srt
Description: SRT library set
Version: ${SRT_VERSION}

Libs: -L\${libdir} -lsrt
Libs.private: -lm -lstdc++ -lpthread -lws2_32
Cflags: -I\${includedir} -I\${includedir}/srt
Requires: openssl libcrypto
EOF
}

create_tesseract_package_config() {
  local TESSERACT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/tesseract.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/tesseract
exec_prefix=\${prefix}
bindir=\${exec_prefix}/bin
datarootdir=\${prefix}/share
datadir=\${datarootdir}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: tesseract
Description: An OCR Engine that was developed at HP Labs between 1985 and 1995... and now at Google.
URL: https://github.com/tesseract-ocr/tesseract
Version: ${TESSERACT_VERSION}

Requires: lept, libjpeg, libpng, giflib, zlib, libwebp, libtiff-4
Libs: -L\${libdir} -ltesseract -lstdc++
Cflags: -I\${includedir}
EOF
}

create_x265_package_config() {
  local X265_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/x265.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/x265
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: x265
Description: H.265/HEVC video encoder
Version: ${X265_VERSION}

Libs: -L\${libdir} -lx265
Libs.private: -lm -lstdc++
Cflags: -I\${includedir}
EOF
}

create_xvidcore_package_config() {
  local XVIDCORE_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/xvidcore.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/xvidcore
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: xvidcore
Description: the main MPEG-4 de-/encoding library
Version: ${XVIDCORE_VERSION}

Requires:
Libs: -L\${libdir} -lxvidcore
Cflags: -I\${includedir}
EOF
}

create_zimg_package_config() {
  local ZIMG_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/zimg.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/zimg
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zimg
Description: Scaling, colorspace conversion, and dithering library
Version: ${ZIMG_VERSION}

Libs: -L\${libdir} -lzimg -lstdc++
Cflags: -I\${includedir}
EOF
}

create_zlib_package_config() {
  local ZLIB_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/zlib.pc" <<EOF
prefix=$(get_native_path "${LIB_INSTALL_BASE}")/zlib
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zlib
Description: zlib compression library
Version: ${ZLIB_VERSION}

Requires:
Libs: -L\${libdir} -lz
Cflags: -I\${includedir}
EOF
}

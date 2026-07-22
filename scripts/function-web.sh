#!/bin/bash

source "${BASEDIR}/scripts/function.sh"

prepare_inline_sed || exit 1

enable_default_web_architectures() {
  ENABLED_ARCHITECTURES[ARCH_WASM32]=1
}

get_ffmpeg_kit_version() {
  local FFMPEG_KIT_VERSION
  FFMPEG_KIT_VERSION=$(sed -n 's/.*FFmpegKitVersion = "\([^"]*\)".*/\1/p' "${BASEDIR}/web/src/FFmpegKitConfig.h" 2>>"${BASEDIR}"/build.log)

  echo "${FFMPEG_KIT_VERSION}"
}

get_build_directory() {
  echo "web-wasm32"
}

get_bundle_directory() {
  if web_linkage_is_static; then
    echo "bundle-web-wasm32-static"
  else
    echo "bundle-web-wasm32"
  fi
}

get_web_linkage_mode() {
  echo "${FFMPEG_KIT_WEB_LINKAGE:-dynamic}"
}

web_linkage_is_static() {
  [[ $(get_web_linkage_mode) == "static" ]]
}

validate_web_linkage_mode() {
  case "$(get_web_linkage_mode)" in
  dynamic | static)
    return 0
    ;;
  *)
    echo -e "\n(*) Invalid web linkage mode '${FFMPEG_KIT_WEB_LINKAGE}'. Use 'dynamic' or 'static'.\n"
    return 1
    ;;
  esac
}

get_target_cpu() {
  echo "wasm32"
}

get_cmake_system_processor() {
  echo "wasm32"
}

get_target() {
  echo "${FFMPEG_KIT_WEB_TARGET:-wasm32-unknown-emscripten}"
}

get_host() {
  echo "${FFMPEG_KIT_WEB_TARGET:-wasm32-unknown-emscripten}"
}

get_meson_target_host_family() {
  echo "emscripten"
}

get_meson_target_cpu_family() {
  echo "wasm32"
}

get_web_pkg_config_libdir() {
  local PKG_CONFIG_LIBDIR_VALUE="${INSTALL_PKG_CONFIG_DIR}"

  if [[ -n ${FFMPEG_KIT_WEB_PKG_CONFIG_LIBDIR:-} ]]; then
    PKG_CONFIG_LIBDIR_VALUE+=":${FFMPEG_KIT_WEB_PKG_CONFIG_LIBDIR}"
  fi

  echo "${PKG_CONFIG_LIBDIR_VALUE}"
}

get_web_thread_cflags() {
  if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} == "1" ]]; then
    echo "-pthread"
  fi
}

get_web_simd_cflags() {
  if [[ ${FFMPEG_KIT_WEB_RELAXED_SIMD:-0} == "1" ]]; then
    echo "-msimd128 -mrelaxed-simd"
  else
    echo "-msimd128"
  fi
}

get_common_includes() {
  echo ""
}

get_common_cflags() {
  # -sSUPPORT_LONGJMP=wasm: C libraries that use setjmp/longjmp (e.g. libvpx) must
  # use wasm-based SjLj to match the wasm exception handling used by the wrapper and
  # the main module (-fwasm-exceptions). Emscripten forbids mixing wasm exceptions
  # with the default emscripten/JS SjLj, which otherwise leaves emscripten_longjmp
  # undefined at the final link.
  #
  # -sWASM_LEGACY_EXCEPTIONS=0 must also be set here: wasm SjLj emits exception-handling
  # opcodes at compile time, so without this flag the C libraries emit *legacy* EH
  # opcodes (the emscripten default) while the C++ wrapper, the main module, and the
  # link step all emit *new* EH opcodes. Linking the two produces a module that mixes
  # legacy and new exception handling, which the browser refuses to instantiate
  # ("module uses a mix of legacy and new exception handling instructions").
  echo "-fstrict-aliasing -fPIC -DWEB -DFFMPEG_KIT_WEB -sSUPPORT_LONGJMP=wasm -sWASM_LEGACY_EXCEPTIONS=0"
}

get_arch_specific_cflags() {
  echo "-DFFMPEG_KIT_WASM32 -sSUPPORT_LONGJMP=wasm $(get_web_simd_cflags) $(get_web_thread_cflags)"
}

get_size_optimization_cflags() {
  if [[ -z ${FFMPEG_KIT_OPTIMIZED_FOR_SPEED} ]]; then
    local OPTIMIZATION_LEVEL="-O2"
  else
    local OPTIMIZATION_LEVEL="-O3"
  fi

  case $1 in
  ffmpeg)
    echo "${OPTIMIZATION_LEVEL} -ffunction-sections -fdata-sections"
    ;;
  *)
    echo "${OPTIMIZATION_LEVEL} -ffunction-sections -fdata-sections"
    ;;
  esac
}

get_app_specific_cflags() {
  case $1 in
  ffmpeg)
    echo "-Wno-unused-function"
    ;;
  ffmpeg-kit)
    echo "-Wno-unused-function -Wno-pointer-sign -Wno-switch -Wno-deprecated-declarations"
    ;;
  libvpx)
    # clock_gettime / CLOCK_MONOTONIC (used by vpx_ports/vpx_timer.h) are POSIX symbols
    # that Emscripten's musl headers hide under the strict -std=gnu99 in the web CFLAGS.
    # Re-enable the POSIX.1-2001 declarations for this library.
    echo "-D_POSIX_C_SOURCE=200112L -std=gnu99 -Wno-unused-function"
    ;;
  *)
    echo "-std=gnu99 -Wno-unused-function"
    ;;
  esac
}

get_cflags() {
  local ARCH_FLAGS
  local APP_FLAGS
  local COMMON_FLAGS
  local OPTIMIZATION_FLAGS
  local COMMON_INCLUDES

  ARCH_FLAGS=$(get_arch_specific_cflags)
  APP_FLAGS=$(get_app_specific_cflags "$1")
  COMMON_FLAGS=$(get_common_cflags)
  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    OPTIMIZATION_FLAGS=$(get_size_optimization_cflags "$1")
  else
    OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi
  COMMON_INCLUDES=$(get_common_includes)

  echo "${ARCH_FLAGS} ${APP_FLAGS} ${COMMON_FLAGS} ${OPTIMIZATION_FLAGS} ${COMMON_INCLUDES} ${EXTRA_CFLAGS}"
}

get_cxxflags() {
  local OPTIMIZATION_FLAGS
  local BUILD_DATE
  local USES_FFMPEG_KIT_PROTOCOLS

  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    if [[ -z ${FFMPEG_KIT_OPTIMIZED_FOR_SPEED} ]]; then
      OPTIMIZATION_FLAGS="-O2 -ffunction-sections -fdata-sections"
    else
      OPTIMIZATION_FLAGS="-O3 -ffunction-sections -fdata-sections"
    fi
  else
    OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi

  BUILD_DATE="-DFFMPEG_KIT_BUILD_DATE=$(date +%Y%m%d 2>>"${BASEDIR}"/build.log)"
  if [[ -z ${NO_FFMPEG_KIT_PROTOCOLS} ]]; then
    USES_FFMPEG_KIT_PROTOCOLS="-DUSES_FFMPEG_KIT_PROTOCOLS"
  else
    USES_FFMPEG_KIT_PROTOCOLS=""
  fi

  case $1 in
  ffmpeg)
    echo "$(get_web_simd_cflags) $(get_web_thread_cflags) -std=c++17 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS}"
    ;;
  ffmpeg-kit)
    echo "$(get_arch_specific_cflags) -std=c++17 -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE} ${USES_FFMPEG_KIT_PROTOCOLS}"
    ;;
  opencore-amr)
    echo "$(get_arch_specific_cflags) -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE}"
    ;;
  *)
    echo "$(get_arch_specific_cflags) -std=c++17 -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 ${OPTIMIZATION_FLAGS} ${EXTRA_CXXFLAGS} ${BUILD_DATE}"
    ;;
  esac
}

get_ldflags() {
  local OPTIMIZATION_FLAGS

  if [[ -z ${FFMPEG_KIT_DEBUG} ]]; then
    if [[ -z ${FFMPEG_KIT_OPTIMIZED_FOR_SPEED} ]]; then
      OPTIMIZATION_FLAGS="-O2"
    else
      OPTIMIZATION_FLAGS="-O3"
    fi
  else
    OPTIMIZATION_FLAGS="${FFMPEG_KIT_DEBUG}"
  fi

  # -sSUPPORT_LONGJMP=wasm must match the compile-time setting (see get_common_cflags)
  # so every link step (FFmpeg side modules, the main module) uses wasm-based SjLj.
  # -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 select the standard wasm exception
  # model at link time, matching how the C++ libraries and the main module are
  # compiled (get_cxxflags). Both are required: the legacy flag picks the EH model,
  # and -fwasm-exceptions makes emcc link the exception-enabled libc++abi variant
  # (the default -noexcept variant omits __cpp_exception, so FFmpeg's configure
  # test links against an exception-using C++ library like rubberband would fail).
  echo "$(get_web_simd_cflags) $(get_web_thread_cflags) -sSUPPORT_LONGJMP=wasm -fwasm-exceptions -sWASM_LEGACY_EXCEPTIONS=0 ${OPTIMIZATION_FLAGS} ${EXTRA_LDFLAGS}"
}

set_toolchain_paths() {
  export CC=$(command -v emcc)
  export CXX=$(command -v em++)
  export AS="${CC}"
  export AR=$(command -v emar)
  export LD="${CC}"
  export RANLIB=$(command -v emranlib)
  export STRIP=$(command -v emstrip)
  export NM=$(command -v emnm)

  export INSTALL_PKG_CONFIG_DIR="${BASEDIR}/prebuilt/$(get_build_directory)/pkgconfig"
  export LIB_ICONV_PACKAGE_CONFIG_PATH="${INSTALL_PKG_CONFIG_DIR}/libiconv.pc"
  export ZLIB_PACKAGE_CONFIG_PATH="${INSTALL_PKG_CONFIG_DIR}/zlib.pc"

  if [ ! -d "${INSTALL_PKG_CONFIG_DIR}" ]; then
    mkdir -p "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1
  fi

  if [ ! -f "${LIB_ICONV_PACKAGE_CONFIG_PATH}" ]; then
    create_libiconv_system_package_config
  fi

  if [ ! -f "${ZLIB_PACKAGE_CONFIG_PATH}" ]; then
    create_zlib_system_package_config
  fi
}

copy_web_config_sub() {
  local CONFIG_SOURCE="${FFMPEG_KIT_TMPDIR}/source/config"

  if [[ -f "${CONFIG_SOURCE}/config.guess" && -f "${CONFIG_SOURCE}/config.sub" ]]; then
    overwrite_file "${CONFIG_SOURCE}/config.guess" "${BASEDIR}/web/config.guess" || return 1
    overwrite_file "${CONFIG_SOURCE}/config.sub" "${BASEDIR}/web/config.sub" || return 1
  fi
}

prepare_rapidjson_headers() {
  RAPIDJSON_INCLUDE_BASE="${FFMPEG_KIT_TMPDIR}/source/rapidjson/include"

  if [ ! -d "${RAPIDJSON_INCLUDE_BASE}/rapidjson" ]; then
    echo -e "\nERROR: rapidjson headers not found at ${RAPIDJSON_INCLUDE_BASE}. Run download_rapidjson first.\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  echo -e "INFO: Using rapidjson headers at ${RAPIDJSON_INCLUDE_BASE}/rapidjson\n" 1>>"${BASEDIR}"/build.log 2>&1
}

create_giflib_package_config() {
  local GIFLIB_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/giflib.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/giflib
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
prefix="${LIB_INSTALL_BASE}"/gmp
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
prefix="${LIB_INSTALL_BASE}"/gnutls
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: gnutls
Description: GNU TLS Implementation

Version: ${GNUTLS_VERSION}
Requires: nettle, hogweed
Cflags: -I\${includedir}
Libs: -L\${libdir} -lgnutls
Libs.private: -lgmp
EOF
}

# Convert a space-separated flag string to a meson array literal
to_meson_array() {
  local out=""
  local token
  for token in $1; do
    out+="'${token}', "
  done
  echo "[${out%, }]"
}

create_mason_cross_file() {
  # Unlike autotools, meson ignores the CFLAGS/CXXFLAGS/LDFLAGS environment in
  # cross builds, so the web flags (-fPIC, -pthread, -msimd128, wasm exceptions)
  # must be embedded here or the objects cannot be linked into side modules.
  cat >"$1" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = 'pkg-config'

[properties]
has_function_printf = true
needs_exe_wrapper = true
pkg_config_libdir = '$(get_web_pkg_config_libdir)'

[host_machine]
system = '$(get_meson_target_host_family)'
cpu_family = '$(get_meson_target_cpu_family)'
cpu = '$(get_cmake_system_processor)'
endian = 'little'

[built-in options]
default_library = 'static'
prefix = '${LIB_INSTALL_PREFIX}'
c_args = $(to_meson_array "${CFLAGS}")
cpp_args = $(to_meson_array "${CXXFLAGS}")
c_link_args = $(to_meson_array "${LDFLAGS}")
cpp_link_args = $(to_meson_array "${LDFLAGS}")
EOF
}

create_freetype_package_config() {
  local FREETYPE_VERSION="$1"

  # No zlib/libbrotlidec requires (unlike Android): web freetype uses Emscripten's
  # zlib port (-sUSE_ZLIB=1, added by the ffmpeg script) and builds without brotli.
  cat >"${INSTALL_PKG_CONFIG_DIR}/freetype2.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/freetype
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: FreeType 2
URL: https://freetype.org
Description: A free, high-quality, and portable font engine.
Version: ${FREETYPE_VERSION}
Requires: libpng, libbrotlidec, zlib
Libs: -L\${libdir} -lfreetype
Cflags: -I\${includedir}/freetype2
EOF
}

create_fontconfig_package_config() {
  local FONTCONFIG_VERSION="$1"
  local REQUIRES="freetype2 >= 21.0.15, expat >= 2.2.0"

  if [[ ${ENABLED_LIBRARIES[LIBRARY_WEB_LIBICONV]} -eq 1 ]]; then
    REQUIRES+=", libiconv"
  fi

  # No uuid require (unlike Android): fontconfig 2.18 does not use uuid.
  cat >"${INSTALL_PKG_CONFIG_DIR}/fontconfig.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/fontconfig
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
Requires: ${REQUIRES}
Libs: -L\${libdir} -lfontconfig
Cflags: -I\${includedir}
EOF
}

create_tesseract_package_config() {
  local TESSERACT_VERSION="$1"

  # No "zlib" in Requires (unlike Android): zlib comes from Emscripten's port and
  # has no .pc; -lc++_shared is dropped because the C++ runtime resolves at the
  # main-module link.
  cat >"${INSTALL_PKG_CONFIG_DIR}/tesseract.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/tesseract
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

Requires: lept, libjpeg, libpng, giflib, libwebp, libtiff-4
Libs: -L\${libdir} -ltesseract
Cflags: -I\${includedir}
EOF
}

create_xvidcore_package_config() {
  local XVIDCORE_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/xvidcore.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/xvidcore
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: xvidcore
Description: the main MPEG-4 de-/encoding library
Version: ${XVIDCORE_VERSION}

Requires:
Libs: -L\${libdir}
Cflags: -I\${includedir}
EOF
}

create_libxml2_package_config() {
  local LIBXML2_VERSION="$1"
  local REQUIRES="libiconv, zlib"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libxml-2.0.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/libxml2
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include
modules=1

Name: libXML
Version: ${LIBXML2_VERSION}
Description: libXML library version2.
Requires: ${REQUIRES}
Libs: -L\${libdir} -lxml2
Libs.private: -lm
Cflags: -I\${includedir} -I\${includedir}/libxml2
EOF
}

create_chromaprint_package_config() {
  local CHROMAPRINT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libchromaprint.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/chromaprint
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: chromaprint
Description: Audio fingerprint library
URL: http://acoustid.org/chromaprint
Version: ${CHROMAPRINT_VERSION}
Libs: -L\${libdir} -lchromaprint
Libs.private: -lm
Cflags: -I\${includedir}
EOF
}

create_snappy_package_config() {
  local SNAPPY_VERSION="$1"

  # No -lz (unlike Android): web snappy builds with -DHAVE_LIBZ=0, and the C++
  # runtime resolves at the main-module link.
  cat >"${INSTALL_PKG_CONFIG_DIR}/snappy.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/snappy
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: snappy
Description: a fast compressor/decompressor
Version: ${SNAPPY_VERSION}

Requires:
Libs: -L\${libdir} -lsnappy
Cflags: -I\${includedir}
EOF
}

create_soxr_package_config() {
  local SOXR_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/soxr.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/soxr
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
prefix="${LIB_INSTALL_BASE}"/srt
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: srt
Description: SRT library set
Version: ${SRT_VERSION}

Libs: -L\${libdir} -lsrt
Libs.private:
Cflags: -I\${includedir} -I\${includedir}/srt
Requires.private: openssl libcrypto
EOF
}

create_libmp3lame_package_config() {
  local LAME_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/libmp3lame.pc" <<EOF
prefix="${LIB_INSTALL_BASE}"/lame
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

create_libiconv_system_package_config() {
  local EMSCRIPTEN_PREFIX="/usr"
  local EMSCRIPTEN_INCLUDE_DIR

  if [[ -n ${CC:-} ]]; then
    EMSCRIPTEN_INCLUDE_DIR=$("${CC}" --print-file-name=include 2>>"${BASEDIR}"/build.log)
    if [[ -n ${EMSCRIPTEN_INCLUDE_DIR} && ${EMSCRIPTEN_INCLUDE_DIR} != "include" ]]; then
      EMSCRIPTEN_PREFIX="${EMSCRIPTEN_INCLUDE_DIR%/include}"
    fi
  fi

  cat >"${INSTALL_PKG_CONFIG_DIR}/libiconv.pc" <<EOF
prefix=${EMSCRIPTEN_PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: libiconv
Description: POSIX iconv API provided by Emscripten musl libc
Version: 0

Libs:
Cflags:
EOF
}

create_zlib_system_package_config() {
  local EMSCRIPTEN_PREFIX="/usr"
  local EMSCRIPTEN_INCLUDE_DIR

  if [[ -n ${CC:-} ]]; then
    EMSCRIPTEN_INCLUDE_DIR=$("${CC}" --print-file-name=include 2>>"${BASEDIR}"/build.log)
    if [[ -n ${EMSCRIPTEN_INCLUDE_DIR} && ${EMSCRIPTEN_INCLUDE_DIR} != "include" ]]; then
      EMSCRIPTEN_PREFIX="${EMSCRIPTEN_INCLUDE_DIR%/include}"
    fi
  fi

  # zlib is not part of Emscripten's libc; it is supplied by the zlib port. The
  # -sUSE_ZLIB=1 flag (emitted here as both cflags and libs) makes Emscripten add
  # the port's headers at compile time and link the port at link time.
  cat >"${INSTALL_PKG_CONFIG_DIR}/zlib.pc" <<EOF
prefix=${EMSCRIPTEN_PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: zlib
Description: zlib compression library provided by the Emscripten zlib port
Version: 1.3.1

Libs: -sUSE_ZLIB=1
Cflags: -sUSE_ZLIB=1
EOF
}

create_ffmpegkit_package_config() {
  local FFMPEGKIT_VERSION="$1"

  cat >"${INSTALL_PKG_CONFIG_DIR}/ffmpeg-kit-next.pc" <<EOF
prefix=${LIB_INSTALL_BASE}/ffmpeg-kit
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: ffmpeg-kit-next
Description: FFmpeg for web applications
Version: ${FFMPEGKIT_VERSION}

Libs: -L\${libdir} -lffmpegkit
Requires: libavdevice, libavfilter, libswscale, libavformat, libavcodec, libswresample, libavutil
Cflags: -I\${includedir}
EOF
}

get_web_bundle_library_search_flags() {
  local lib_dir

  for lib_dir in "${LIB_INSTALL_BASE}"/*/lib; do
    if [[ -d "${lib_dir}" ]]; then
      printf ' -L%s' "${lib_dir}"
    fi
  done
}

get_web_ffmpeg_external_ldflags() {
  local FFMPEG_PKG_CONFIG_LIBS

  FFMPEG_PKG_CONFIG_LIBS=$(
    PKG_CONFIG_LIBDIR="$(get_web_pkg_config_libdir)" pkg-config --libs --static \
      libavdevice libavfilter libswscale libavformat libavcodec libswresample libavutil \
      2>>"${BASEDIR}"/build.log
  ) || return 1

  printf '%s\n' "${FFMPEG_PKG_CONFIG_LIBS}" | awk '
    {
      for (i = 1; i <= NF; i++) {
        t = $i
        if (t ~ /^-l(avdevice|avfilter|swscale|avformat|avcodec|swresample|avutil)$/) continue
        if (t == "-lstdc++" || t == "-lc++" || t == "-lpthread") continue
        if (t ~ /^-sPTHREAD_POOL_SIZE=/ || t ~ /^-sUSE_ZLIB=/) continue
        if (seen[t]++) continue
        out = (out == "" ? t : out " " t)
      }
    }
    END {
      print out
    }
  '
}

install_web_pkg_config_file() {
  local FILE_NAME="$1"
  local SOURCE="${INSTALL_PKG_CONFIG_DIR}/${FILE_NAME}"
  local DESTINATION="${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}/${FILE_NAME}"

  rm -f "$DESTINATION" 2>>"${BASEDIR}"/build.log
  cp "$SOURCE" "$DESTINATION" 2>>"${BASEDIR}"/build.log || return 1

  ${SED_INLINE} "s|${LIB_INSTALL_BASE}/ffmpeg-kit|${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next|g" "$DESTINATION" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} "s|${LIB_INSTALL_BASE}/ffmpeg|${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next|g" "$DESTINATION" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
}

create_web_main_module() {
  local FFMPEG_KIT_BUNDLE_LIB_DIRECTORY="$1"
  local FFMPEG_EXTERNAL_LDFLAGS
  local FFMPEG_EXTERNAL_LDFLAGS_ARRAY=()
  local FFMPEG_LIBRARY_SEARCH_FLAGS
  local EXPORTED_RUNTIME_METHODS
  local LINK_INPUTS=()
  local LINK_TRAILER=()
  local MAIN_MODULE_FLAGS=()
  local REQUIRED_LIBRARY
  local WEB_LINKAGE_MODE

  WEB_LINKAGE_MODE="$(get_web_linkage_mode)"

  if web_linkage_is_static; then
    REQUIRED_LIBRARY="${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.a"
    EXPORTED_RUNTIME_METHODS="ccall,cwrap,FS"
    LINK_INPUTS=(
      -Wl,--whole-archive
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.a"
      -Wl,--no-whole-archive
      -Wl,--start-group
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavdevice.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavcodec.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavfilter.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavformat.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavutil.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libswresample.a"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libswscale.a"
    )
    LINK_TRAILER=(-Wl,--end-group)
  else
    REQUIRED_LIBRARY="${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.so"
    EXPORTED_RUNTIME_METHODS="ccall,cwrap,FS,callMain"
    MAIN_MODULE_FLAGS=(-sMAIN_MODULE=2)
    LINK_INPUTS=(
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavdevice.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavcodec.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavfilter.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavformat.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libavutil.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libswresample.so"
      "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libswscale.so"
    )
  fi

  if [[ ! -f "${REQUIRED_LIBRARY}" ]]; then
    echo -e "INFO: Skipping libffmpegkit ${WEB_LINKAGE_MODE} main module; ${REQUIRED_LIBRARY##*/} not found (ffmpeg-kit build skipped?)\n" 1>>"${BASEDIR}"/build.log 2>&1
    return
  fi

  echo -e "INFO: Creating libffmpegkit.js and libffmpegkit.wasm ${WEB_LINKAGE_MODE} main module\n" 1>>"${BASEDIR}"/build.log 2>&1

  # Extra diagnostics for --debug builds (FFMPEG_KIT_DEBUG is set by enable_debug):
  # runtime assertions, a stack-overflow guard, and function names in stack traces.
  local DEBUG_FLAGS=""
  if [[ -n ${FFMPEG_KIT_DEBUG} ]]; then
    DEBUG_FLAGS="-sASSERTIONS=2 -sSTACK_OVERFLOW_CHECK=2 -gline-tables-only"
  fi

  FFMPEG_EXTERNAL_LDFLAGS="$(get_web_ffmpeg_external_ldflags)" || return 1
  if [[ -n ${FFMPEG_EXTERNAL_LDFLAGS} ]]; then
    read -r -a FFMPEG_EXTERNAL_LDFLAGS_ARRAY <<<"${FFMPEG_EXTERNAL_LDFLAGS}"
  fi
  FFMPEG_LIBRARY_SEARCH_FLAGS="$(get_web_bundle_library_search_flags)"

  "${CXX}" \
    $(get_ldflags ffmpeg-kit) \
    -fwasm-exceptions \
    -sWASM_LEGACY_EXCEPTIONS=0 \
    ${DEBUG_FLAGS} \
    -L"${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" \
    ${FFMPEG_LIBRARY_SEARCH_FLAGS} \
    "${MAIN_MODULE_FLAGS[@]}" \
    -sEXPORT_ES6=1 \
    -sMODULARIZE=1 \
    -sEXPORT_NAME=FFmpegKitModule \
    -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=64MB \
    -sMAXIMUM_MEMORY=4GB \
    -sSTACK_SIZE=5MB \
    -sFORCE_FILESYSTEM=1 \
    -sEXPORTED_RUNTIME_METHODS="${EXPORTED_RUNTIME_METHODS}" \
    -sENVIRONMENT=web,worker \
    -sWASM_BIGINT=1 \
    -sUSE_ZLIB=1 \
    -lembind \
    -lworkerfs.js \
    -o "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.js" \
    "${LINK_INPUTS[@]}" \
    "${FFMPEG_EXTERNAL_LDFLAGS_ARRAY[@]}" \
    "${LINK_TRAILER[@]}" \
    1>>"${BASEDIR}"/build.log 2>&1 || return 1

  echo -e "\nINFO: Created libffmpegkit.js and libffmpegkit.wasm ${WEB_LINKAGE_MODE} main module\n" 1>>"${BASEDIR}"/build.log 2>&1

}

create_web_bundle() {
  set_toolchain_paths ""

  local FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/include"
  local FFMPEG_KIT_BUNDLE_LIB_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/lib"
  FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next/pkgconfig"

  initialize_folder "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}" || return 1
  initialize_folder "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" || return 1
  initialize_folder "${FFMPEG_KIT_BUNDLE_PKG_CONFIG_DIRECTORY}" || return 1

  if [[ -d "${LIB_INSTALL_BASE}/ffmpeg-kit/include" ]]; then
    cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg-kit/include/* "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}" 2>>"${BASEDIR}"/build.log
  fi
  cp -r -P "${LIB_INSTALL_BASE}"/ffmpeg/include/* "${FFMPEG_KIT_BUNDLE_INCLUDE_DIRECTORY}" 2>>"${BASEDIR}"/build.log

  if web_linkage_is_static; then
    local WEB_LIBRARY_SUFFIX="a"
  else
    local WEB_LIBRARY_SUFFIX="so"
  fi

  if [[ -f "${LIB_INSTALL_BASE}/ffmpeg-kit/lib/libffmpegkit.${WEB_LIBRARY_SUFFIX}" ]]; then
    cp -L "${LIB_INSTALL_BASE}/ffmpeg-kit/lib/libffmpegkit.${WEB_LIBRARY_SUFFIX}" "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/libffmpegkit.${WEB_LIBRARY_SUFFIX}" 2>>"${BASEDIR}"/build.log || return 1
  fi
  for library in libavdevice libavcodec libavfilter libavformat libavutil libswresample libswscale; do
    cp -L "${LIB_INSTALL_BASE}/ffmpeg/lib/${library}.${WEB_LIBRARY_SUFFIX}" "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}/${library}.${WEB_LIBRARY_SUFFIX}" 2>>"${BASEDIR}"/build.log || return 1
  done

  create_web_main_module "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}" || return 1

  # Ship the hand-written JS binding layer (public native-named API + the internal
  # FFmpegKitFactory conduit and worker host) plus its TypeScript declarations next to
  # lib/. The worker imports ../lib/libffmpegkit.js, so binding/ and lib/ must remain
  # siblings in the bundle. The npm package is rooted at the bundle's ffmpeg-kit-next/
  # directory (package.json below), so binding/ and lib/ both fall under the package.
  local FFMPEG_KIT_BUNDLE_ROOT="${BASEDIR}/prebuilt/$(get_bundle_directory)/ffmpeg-kit-next"
  local FFMPEG_KIT_BUNDLE_BINDING_DIRECTORY="${FFMPEG_KIT_BUNDLE_ROOT}/binding"
  initialize_folder "${FFMPEG_KIT_BUNDLE_BINDING_DIRECTORY}" || return 1
  cp "${BASEDIR}"/web/binding/*.js "${BASEDIR}"/web/binding/*.d.ts "${FFMPEG_KIT_BUNDLE_BINDING_DIRECTORY}" 2>>"${BASEDIR}"/build.log || return 1
  cp "${BASEDIR}"/web/package.json "${FFMPEG_KIT_BUNDLE_ROOT}/package.json" 2>>"${BASEDIR}"/build.log || return 1

  for pc_file in libavformat.pc libswresample.pc libswscale.pc libavdevice.pc libavfilter.pc libavcodec.pc libavutil.pc; do
    install_web_pkg_config_file "${pc_file}" || return 1
  done

  if [[ -f "${INSTALL_PKG_CONFIG_DIR}/ffmpeg-kit-next.pc" ]]; then
    install_web_pkg_config_file "ffmpeg-kit-next.pc" || return 1
  fi

  if [[ ${GPL_ENABLED} == "yes" ]]; then
    cp "${BASEDIR}"/tools/license/LICENSE.GPLv3 "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  else
    cp "${BASEDIR}"/LICENSE "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}"/license.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  fi

  cp "${BASEDIR}"/tools/source/SOURCE "${FFMPEG_KIT_BUNDLE_LIB_DIRECTORY}"/source.txt 1>>"${BASEDIR}"/build.log 2>&1 || return 1
}

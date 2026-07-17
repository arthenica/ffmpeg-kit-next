#!/bin/bash

# ENABLE COMMON FUNCTIONS
source "${BASEDIR}"/scripts/function-"${FFMPEG_KIT_BUILD_TYPE}".sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1

LIB_NAME="ffmpeg-kit"

if [[ ${FFMPEG_KIT_WEB_PTHREADS:-1} != "1" ]]; then
  echo -e "\nERROR: The current web ffmpeg-kit wrapper build requires Emscripten pthreads. Re-run with --enable-pthreads or use --skip-ffmpeg-kit for an FFmpeg-core-only build.\n" 1>>"${BASEDIR}"/build.log 2>&1
  exit 1
fi

echo -e "----------------------------------------------------------------" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "\nINFO: Building ${LIB_NAME} for ${HOST} with the following environment variables\n" 1>>"${BASEDIR}"/build.log 2>&1
env 1>>"${BASEDIR}"/build.log 2>&1
echo -e "----------------------------------------------------------------\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "INFO: System information\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "INFO: $(uname -a)\n" 1>>"${BASEDIR}"/build.log 2>&1
echo -e "----------------------------------------------------------------\n" 1>>"${BASEDIR}"/build.log 2>&1

FFMPEG_KIT_LIBRARY_PATH="${LIB_INSTALL_BASE}/${LIB_NAME}"

# SET PATHS
set_toolchain_paths "${LIB_NAME}"

# SET BUILD FLAGS
HOST=$(get_host)
export PKG_CONFIG_LIBDIR="$(get_web_pkg_config_libdir)"
unset PKG_CONFIG_PATH

prepare_rapidjson_headers || return 1
export CFLAGS="$(get_cflags ${LIB_NAME}) -I${LIB_INSTALL_BASE}/ffmpeg/include"
# RAPIDJSON_INCLUDE_BASE IS LISTED FIRST SO THAT OUR PINNED HEADERS TAKE PRECEDENCE
export CXXFLAGS="$(get_cxxflags ${LIB_NAME}) -I${RAPIDJSON_INCLUDE_BASE} -I${LIB_INSTALL_BASE}/ffmpeg/include"
export LDFLAGS="$(get_ldflags ${LIB_NAME}) -L${LIB_INSTALL_BASE}/ffmpeg/lib -sSIDE_MODULE=1"

cd "${BASEDIR}"/web 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# WEB SHARED LIBRARIES ARE EMSCRIPTEN SIDE MODULES LOADED BY A MAIN MODULE.
BUILD_LIBRARY_OPTIONS="--enable-shared --disable-static"

echo -n -e "\n${LIB_NAME}: "

make distclean 2>/dev/null 1>/dev/null

rm -f "${BASEDIR}"/web/src/libffmpegkit* 1>>"${BASEDIR}"/build.log 2>&1

# ALWAYS REGENERATE BUILD FILES - NECESSARY TO APPLY THE WORKAROUNDS
copy_web_config_sub 1>>"${BASEDIR}"/build.log 2>&1 || return 1
autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emconfigure ./configure \
  --prefix="${FFMPEG_KIT_LIBRARY_PATH}" \
  --with-pic \
  ${BUILD_LIBRARY_OPTIONS} \
  --disable-fast-install \
  --disable-maintainer-mode \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1

if [ $? -ne 0 ]; then
  exit 1
fi

# DELETE THE PREVIOUS BUILD OF THE LIBRARY
if [ -d "${FFMPEG_KIT_LIBRARY_PATH}" ]; then
  rm -rf "${FFMPEG_KIT_LIBRARY_PATH}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emmake make -j$(get_cpu_count) install 1>>"${BASEDIR}"/build.log 2>&1

if [ $? -eq 0 ]; then
  echo "ok"
else
  exit 1
fi

# CREATE PACKAGE CONFIG MANUALLY
create_ffmpegkit_package_config "$(get_ffmpeg_kit_version)" || return 1

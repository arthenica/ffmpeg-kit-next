#!/bin/bash

# ENABLE COMMON FUNCTIONS
source "${BASEDIR}"/scripts/function-"${FFMPEG_KIT_BUILD_TYPE}".sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1

LIB_NAME="ffmpeg-kit"

prepare_rapidjson_headers() {
  RAPIDJSON_INCLUDE_BASE="${FFMPEG_KIT_TMPDIR}/source/rapidjson/include"

  if [ ! -d "${RAPIDJSON_INCLUDE_BASE}/rapidjson" ]; then
    echo -e "\nERROR: rapidjson headers not found at ${RAPIDJSON_INCLUDE_BASE}. Run download_rapidjson first.\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  echo -e "INFO: Using rapidjson headers at ${RAPIDJSON_INCLUDE_BASE}/rapidjson\n" 1>>"${BASEDIR}"/build.log 2>&1
}

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
export PKG_CONFIG_LIBDIR="$(get_pkg_config_libdir)"
unset PKG_CONFIG_PATH

prepare_rapidjson_headers || return 1
export CFLAGS="$(get_cflags ${LIB_NAME}) -I${LIB_INSTALL_BASE}/ffmpeg/include"
# RAPIDJSON_INCLUDE_BASE IS LISTED FIRST SO THAT OUR PINNED HEADERS TAKE PRECEDENCE
export CXXFLAGS="$(get_cxxflags ${LIB_NAME}) -I${RAPIDJSON_INCLUDE_BASE} -I${LIB_INSTALL_BASE}/ffmpeg/include"
export LDFLAGS="$(get_ldflags ${LIB_NAME}) -L${LIB_INSTALL_BASE}/ffmpeg/lib -lavdevice"


cd "${BASEDIR}"/windows 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# WORKAROUND: NEUTRALISE FFMPEG'S WINDOWS COMMAND-LINE OVERRIDE
#
# On Windows, fftools' prepare_app_arguments() discards the (argc, argv) handed to
# ffmpeg_execute()/ffprobe_execute() and rebuilds the arguments from the host
# process command line via GetCommandLineW(). In-process that is the host
# application's command line, not FFmpegKit's, so every command runs as a bare
# "ffmpeg" and prints the usage banner. Flipping the five Win32 guards to a
# disabled '#if 0 && ...' makes prepare_app_arguments the same no-op it already is
# on Linux, so the supplied UTF-8 argv is used directly. Idempotent: the '#if
# HAVE_...' anchor is consumed, so re-running the build does not re-apply it.
for SOURCE_FILE in \
  "${BASEDIR}/windows/src/fftools/cmdutils.c" \
  "${BASEDIR}/windows/src/fftools/cmdutils.h" \
  "${BASEDIR}/windows/src/ffmpeg_context.c" \
  "${BASEDIR}/windows/src/ffmpeg_context.h"; do
  sed -i 's|#if HAVE_COMMANDLINETOARGVW && defined(_WIN32)|#if 0 \&\& HAVE_COMMANDLINETOARGVW \&\& defined(_WIN32)|g' "${SOURCE_FILE}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
done

# ALWAYS BUILD SHARED LIBRARIES
BUILD_LIBRARY_OPTIONS="--enable-shared --disable-static"

# THE MinGW TOOLCHAIN RUNTIME IS LINKED STATICALLY UNLESS --no-static-mingw-runtime
# WAS GIVEN. configure.ac RESOLVES THE ARCHIVES AND FIXES UP libtool ITSELF.
if [[ -n ${NO_STATIC_MINGW_RUNTIME} ]]; then
  BUILD_LIBRARY_OPTIONS+=" --disable-static-mingw-runtime"
fi

echo -n -e "\n${LIB_NAME}: "

make distclean 2>/dev/null 1>/dev/null

rm -f "${BASEDIR}"/windows/src/libffmpegkit* 1>>"${BASEDIR}"/build.log 2>&1

# ALWAYS REGENERATE BUILD FILES - NECESSARY TO APPLY THE WORKAROUNDS
autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

./configure \
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

make -j$(get_cpu_count) install 1>>"${BASEDIR}"/build.log 2>&1

if [ $? -eq 0 ]; then
  echo "ok"
else
  exit 1
fi

# CREATE PACKAGE CONFIG MANUALLY
create_ffmpegkit_package_config "$(get_ffmpeg_kit_version)" || return 1

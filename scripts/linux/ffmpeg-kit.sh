#!/bin/bash

# ENABLE COMMON FUNCTIONS
source "${BASEDIR}"/scripts/function-"${FFMPEG_KIT_BUILD_TYPE}".sh 1>>"${BASEDIR}"/build.log 2>&1 || return 1

LIB_NAME="ffmpeg-kit"

prepare_rapidjson_headers() {
  RAPIDJSON_SYSTEM_INCLUDE_DIR="${RAPIDJSON_SYSTEM_INCLUDE_DIR:-/usr/include/rapidjson}"
  RAPIDJSON_STAGED_INCLUDE_BASE="${FFMPEG_KIT_TMPDIR}/rapidjson/include"
  RAPIDJSON_STAGED_INCLUDE_DIR="${RAPIDJSON_STAGED_INCLUDE_BASE}/rapidjson"

  if [ ! -d "${RAPIDJSON_SYSTEM_INCLUDE_DIR}" ]; then
    echo -e "\nERROR: rapidjson headers not found at ${RAPIDJSON_SYSTEM_INCLUDE_DIR}\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi

  rm -rf "${RAPIDJSON_STAGED_INCLUDE_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  mkdir -p "${RAPIDJSON_STAGED_INCLUDE_BASE}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  cp -R "${RAPIDJSON_SYSTEM_INCLUDE_DIR}" "${RAPIDJSON_STAGED_INCLUDE_BASE}/" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
  ${SED_INLINE} 's|GenericStringRef& operator=(const GenericStringRef& rhs) { s = rhs.s; length = rhs.length; }|GenericStringRef\& operator=(const GenericStringRef\& rhs);|g' "${RAPIDJSON_STAGED_INCLUDE_DIR}/document.h" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

  echo -e "INFO: Using rapidjson headers at ${RAPIDJSON_STAGED_INCLUDE_DIR}\n" 1>>"${BASEDIR}"/build.log 2>&1
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
export PKG_CONFIG_LIBDIR="$(get_linux_pkg_config_libdir)"
unset PKG_CONFIG_PATH

prepare_rapidjson_headers || return 1
export CFLAGS="$(get_cflags ${LIB_NAME}) -I${LIB_INSTALL_BASE}/ffmpeg/include"
export CXXFLAGS="$(get_cxxflags ${LIB_NAME}) -I${LIB_INSTALL_BASE}/ffmpeg/include -I${RAPIDJSON_STAGED_INCLUDE_BASE}"
export LDFLAGS="$(get_ldflags ${LIB_NAME}) -L${LIB_INSTALL_BASE}/ffmpeg/lib -lavdevice"

cd "${BASEDIR}"/linux 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# ALWAYS BUILD SHARED LIBRARIES
BUILD_LIBRARY_OPTIONS="--enable-shared --disable-static"

echo -n -e "\n${LIB_NAME}: "

make distclean 2>/dev/null 1>/dev/null

rm -f "${BASEDIR}"/linux/src/libffmpegkit* 1>>"${BASEDIR}"/build.log 2>&1

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

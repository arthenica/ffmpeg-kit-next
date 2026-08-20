#!/bin/bash

# ENABLE COMMON FUNCTIONS
source "${BASEDIR}"/scripts/function-"${FFMPEG_KIT_BUILD_TYPE}".sh || return 1

LIB_NAME=$1
ENABLED_LIBRARY_PATH="${LIB_INSTALL_BASE}/${LIB_NAME}"

# PREPARE PATHS & DEFINE ${INSTALL_PKG_CONFIG_DIR}
SCRIPT_PATH="${BASEDIR}/scripts/windows/${LIB_NAME}.sh"

set_toolchain_paths "${LIB_NAME}"

# SET BUILD FLAGS
HOST=$(get_host)
export CFLAGS=$(get_cflags "${LIB_NAME}")
export CXXFLAGS=$(get_cxxflags "${LIB_NAME}")
export LDFLAGS=$(get_ldflags "${LIB_NAME}")
export PKG_CONFIG_LIBDIR="$(get_pkg_config_libdir)"
unset PKG_CONFIG_PATH

echo -e "----------------------------------------------------------------"
echo -e "\nINFO: Building ${LIB_NAME} for ${HOST} with the following environment variables\n"
env
echo -e "----------------------------------------------------------------\n"
echo -e "INFO: System information\n"
echo -e "INFO: $(uname -a)\n"
echo -e "----------------------------------------------------------------\n"

cd "${BASEDIR}"/src/"${LIB_NAME}" || return 1

LIB_INSTALL_PREFIX="${ENABLED_LIBRARY_PATH}"
BUILD_DIR=$(get_cmake_build_directory)

# DELETE THE PREVIOUS BUILD OF THE LIBRARY
rm -rf "${LIB_INSTALL_PREFIX}" || return 1
rm -rf "${BUILD_DIR}" || return 1

# EXECUTE BUILD SCRIPT OF EACH ENABLED LIBRARY
source "${SCRIPT_PATH}"

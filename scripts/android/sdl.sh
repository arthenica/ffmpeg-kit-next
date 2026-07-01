#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# PATCH SDL SOURCES
apply_sdl_patch() {
  local PATCH_NAME="$1"
  local PATCH_FILE="${BASEDIR}/tools/patch/make/sdl/${PATCH_NAME}"

  if git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --check "${PATCH_FILE}" 2>/dev/null; then
    git -C "${BASEDIR}"/src/"${LIB_NAME}" apply "${PATCH_FILE}" || return 1
  elif git -C "${BASEDIR}"/src/"${LIB_NAME}" apply --reverse --check "${PATCH_FILE}" 2>/dev/null; then
    echo -e "INFO: skipping sdl ${PATCH_NAME}, already applied\n" 1>>"${BASEDIR}"/build.log 2>&1
  else
    echo -e "ERROR: sdl ${PATCH_NAME} does not apply cleanly\n" 1>>"${BASEDIR}"/build.log 2>&1
    return 1
  fi
}

apply_sdl_patch "configure.android-opensles.patch" || return 1

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_sdl} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --disable-video-x11 \
  --with-sysroot="${ANDROID_SYSROOT}" \
  --enable-static \
  --disable-shared \
  --disable-fast-install \
  --disable-hidapi \
  --disable-sensor \
  --host="${HOST}" || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

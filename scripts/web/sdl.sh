#!/bin/bash

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_sdl} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

# UPDATE CONFIG FILES TO RECOGNIZE wasm32-unknown-emscripten WHEN SDL USES THEM
if [[ -f "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.guess ]]; then
  overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.guess "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.guess 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi
if [[ -f "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.sub ]]; then
  overwrite_file "${FFMPEG_KIT_TMPDIR}"/source/config/config.sub "${BASEDIR}"/src/"${LIB_NAME}"/build-scripts/config.sub 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

emconfigure ./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-pic \
  --enable-static \
  --disable-shared \
  --disable-video-x11 \
  --disable-video-wayland \
  --disable-video-cocoa \
  --disable-video-uikit \
  --disable-video-vulkan \
  --disable-video-metal \
  --disable-video-opengl \
  --disable-video-opengles \
  --disable-render-metal \
  --disable-haptic \
  --disable-hidapi \
  --disable-joystick \
  --disable-sensor \
  --host="${HOST}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

emmake make install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./*.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

#!/bin/bash

prepare_meson_build || return 1

"${MESON:-meson}" setup "${BUILD_DIR}" \
  --cross-file="$CROSS_FILE" \
  --buildtype=release \
  --default-library=static \
  -Db_staticpic=true \
  -Db_lto=false \
  -Db_ndebug=if-release \
  -Dfft=builtin \
  -Dresampler=libsamplerate \
  -Dcmdline=disabled \
  -Djni=disabled \
  -Dvamp=disabled \
  -Dladspa=disabled \
  -Dlv2=disabled \
  -Dtests=disabled || return 1

cd "${BUILD_DIR}" || return 1

ninja -j$(get_cpu_count) || return 1

ninja install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path "${BUILD_DIR}"/meson-private/rubberband.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

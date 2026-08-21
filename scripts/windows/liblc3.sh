#!/bin/bash

prepare_meson_build || return 1

"${MESON:-meson}" setup "${BUILD_DIR}" \
  --cross-file="$CROSS_FILE" \
  --buildtype=release \
  -Db_lto=false \
  -Db_ndebug=if-release \
  -Dtools=false \
  -Dpython=false || return 1

cd "${BUILD_DIR}" || return 1

ninja -j$(get_cpu_count) || return 1

ninja install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path "${BUILD_DIR}"/meson-private/lc3.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

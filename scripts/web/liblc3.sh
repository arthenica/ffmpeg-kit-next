#!/bin/bash

# SET BUILD FLAGS
CROSS_FILE="${BASEDIR}"/src/"${LIB_NAME}"/package/crossfiles/$ARCH-$FFMPEG_KIT_BUILD_TYPE.meson

mkdir -p "$(dirname "$CROSS_FILE")" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
create_mason_cross_file "$CROSS_FILE" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
rm -rf "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

"${MESON:-meson}" setup "${BUILD_DIR}" \
  --cross-file="$CROSS_FILE" \
  --buildtype=release \
  --default-library=static \
  -Db_staticpic=true \
  -Db_lto=false \
  -Db_ndebug=if-release \
  -Dtools=false \
  -Dpython=false 1>>"${BASEDIR}"/build.log 2>&1 || return 1

cd "${BUILD_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

ninja -j$(get_cpu_count) 1>>"${BASEDIR}"/build.log 2>&1 || return 1

ninja install 1>>"${BASEDIR}"/build.log 2>&1 || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${BUILD_DIR}"/meson-private/lc3.pc "${INSTALL_PKG_CONFIG_DIR}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1

#!/bin/bash

# SET BUILD FLAGS
CROSS_FILE="${BASEDIR}"/src/"${LIB_NAME}"/package/crossfiles/$ARCH-$FFMPEG_KIT_BUILD_TYPE.meson

mkdir -p "$(dirname "$CROSS_FILE")" || return 1
create_mason_cross_file "$CROSS_FILE" || return 1

"${MESON:-meson}" setup "${BUILD_DIR}" \
  --cross-file="$CROSS_FILE" \
  --default-library=static \
  -Db_staticpic=true \
  -Db_lto=false \
  -Db_ndebug=false \
  -Dfreetype=enabled \
  -Dglib=disabled \
  -Dgobject=disabled \
  -Dcairo=disabled \
  -Dchafa=disabled \
  -Dpng=disabled \
  -Dzlib=disabled \
  -Dicu=disabled \
  -Dgraphite=disabled \
  -Dgraphite2=disabled \
  -Dfontations=disabled \
  -Dgdi=disabled \
  -Ddirectwrite=disabled \
  -Dcoretext=disabled \
  -Dharfrust=disabled \
  -Dkbts=disabled \
  -Dwasm=disabled \
  -Draster=disabled \
  -Dvector=disabled \
  -Dgpu=disabled \
  -Dgpu_demo=disabled \
  -Dsubset=disabled \
  -Dtests=disabled \
  -Dintrospection=disabled \
  -Ddocs=disabled \
  -Dutilities=disabled \
  -Dbenchmark=disabled || return 1

cd "${BUILD_DIR}" || return 1

ninja -j$(get_cpu_count) || return 1

ninja install || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp "${BUILD_DIR}"/meson-private/harfbuzz.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1

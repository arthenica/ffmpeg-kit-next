#!/bin/bash

LIBPLACEBO_SOURCE_DIR="${BASEDIR}/src/${LIB_NAME}"

# INITIALISE THE REQUIRED SUBMODULES FROM THE ARTHENICA FORKS
if [[ $(init_submodules_from_arthenica "${LIBPLACEBO_SOURCE_DIR}" 3rdparty/Vulkan-Headers 3rdparty/jinja 3rdparty/markupsafe 3rdparty/fast_float) -ne 0 ]]; then
  echo -e "ERROR: Failed to initialise the submodules of ${LIB_NAME}\n"
  return 1
fi

# SET BUILD FLAGS
CROSS_FILE="${BASEDIR}"/src/"${LIB_NAME}"/package/crossfiles/$ARCH-$FFMPEG_KIT_BUILD_TYPE.meson

mkdir -p "$(dirname "$CROSS_FILE")" || return 1
create_mason_cross_file "$CROSS_FILE" || return 1

# ALWAYS CLEAN THE PREVIOUS BUILD
rm -rf "${BUILD_DIR}" || return 1

"${MESON:-meson}" setup "${BUILD_DIR}" \
  --cross-file="$CROSS_FILE" \
  --buildtype=release \
  -Db_lto=false \
  -Db_ndebug=if-release \
  -Dvulkan=enabled \
  -Dvk-proc-addr=disabled \
  -Dshaderc=enabled \
  -Dglslang=disabled \
  -Dopengl=disabled \
  -Dd3d11=disabled \
  -Dlcms=disabled \
  -Dlibdovi=disabled \
  -Dxxhash=disabled \
  -Dunwind=disabled \
  -Ddemos=false \
  -Dtests=false \
  -Dbench=false \
  -Dfuzz=false || return 1

cd "${BUILD_DIR}" || return 1

ninja -j$(get_cpu_count) || return 1

ninja install || return 1

# CREATE PACKAGE CONFIG MANUALLY
create_libplacebo_package_config "7.360.1" || return 1

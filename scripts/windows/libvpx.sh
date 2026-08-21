#!/bin/bash

# SET BUILD OPTIONS
# libvpx SHIPS DEDICATED win64 TARGETS. CPU EXTENSIONS AND NEON ARE SELECTED AT
# RUNTIME THROUGH --enable-runtime-cpu-detect.
TARGET=""
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  TARGET="arm64-win64-gcc"
  ASM_OPTIONS="--enable-runtime-cpu-detect"
  ;;
x86-64)
  if ! [ -x "$(command -v nasm)" ]; then
    echo -e "\n(*) nasm command not found\n"
    return 1
  fi

  TARGET="x86_64-win64-gcc"
  ASM_OPTIONS="--as=nasm --enable-runtime-cpu-detect"
  ;;
esac

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# NOTE THAT RECONFIGURE IS NOT SUPPORTED

# UNDO WORKAROUNDS
git checkout "${BASEDIR}"/src/"${LIB_NAME}"/build/make/configure.sh 1>>"${BASEDIR}"/build.log 2>&1

./configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --target="${TARGET}" \
  --extra-cflags="${CFLAGS}" \
  --extra-cxxflags="${CXXFLAGS}" \
  --log=yes \
  --enable-libs \
  --enable-install-libs \
  --enable-pic \
  --enable-optimizations \
  --enable-better-hw-compatibility \
  --enable-vp9-highbitdepth \
  ${ASM_OPTIONS} \
  --enable-vp8 \
  --enable-vp9 \
  --enable-multithread \
  --enable-spatial-resampling \
  --enable-small \
  --enable-static \
  --disable-realtime-only \
  --disable-shared \
  --disable-debug \
  --disable-gprof \
  --disable-gcov \
  --disable-ccache \
  --disable-install-bins \
  --disable-install-srcs \
  --disable-install-docs \
  --disable-docs \
  --disable-tools \
  --disable-examples \
  --disable-unit-tests \
  --disable-decode-perf-tests \
  --disable-encode-perf-tests \
  --disable-codec-srcs \
  --disable-debug-libs \
  --disable-internal-stats || return 1

make -j$(get_cpu_count) || return 1

make install || return 1

# MANUALLY COPY PKG-CONFIG FILES
copy_and_update_path ./*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
